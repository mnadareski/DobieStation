#ifndef GS_HPP
#define GS_HPP
#include <cstdint>
#include <thread>
#include <mutex>
#include "gscontext.hpp"
#include "gsregisters.hpp"
#include "circularFIFO.hpp"

class INTC;

//Commands sent from the main thread to the GS thread.
enum GSCommand:uint8_t 
{
	write64_t, write64_privileged_t, write32_privileged_t,
    set_rgba_t, set_st_t, set_uv_t, set_xyz_t, set_xyzf_t, set_crt_t,
    render_crt_t, assert_finish_t, assert_vsync_t, set_vblank_t, memdump_t, die_t,
    save_state_t, load_state_t, gsdump_t
};

union GSMessagePayload 
{
    struct 
	{
        uint32_t addr;
        uint64_t value;
    } write64_payload;
    struct 
	{
        uint32_t addr;
        uint32_t value;
    } write32_payload;
    struct 
	{
        uint8_t r, g, b, a;
        float q;
    } rgba_payload;
    struct 
	{
        uint32_t s, t;
    } st_payload;
    struct 
	{
        uint16_t u, v;
    } uv_payload;
    struct 
	{
        uint32_t x, y, z;
        bool drawing_kick;
    } xyz_payload;
    struct
    {
        uint32_t x, y, z;
        uint8_t fog;
        bool drawing_kick;
    } xyzf_payload;
    struct 
	{
        bool interlaced;
        int mode;
        bool frame_mode;
    } crt_payload;
    struct 
	{
        bool vblank;
    } vblank_payload;
    struct 
	{
        uint32_t* target;
        std::mutex* target_mutex;
    } render_payload;
    struct
    {
        std::ofstream* state;
    } save_state_payload;
    struct
    {
        std::ifstream* state;
    } load_state_payload;
    struct 
	{
        uint8_t BLANK; 
    } no_payload;//C++ doesn't like the empty struct
};

struct GSMessage
{
    GSCommand type;
    GSMessagePayload payload;
};

//Commands sent from the GS thread to the main thread.
enum GSReturn :uint8_t
{
    render_complete_t,
    death_error_t,
    save_state_done_t,
    load_state_done_t,
    gsdump_render_partial_done_t,
};

union GSReturnMessagePayload
{
    struct
    {
        const char* error_str;
    } death_error_payload;
    struct
    {
        uint16_t x, y;
    } xy_payload;
    struct
    {
        uint8_t BLANK;
    } no_payload;//C++ doesn't like the empty struct
};

struct GSReturnMessage
{
    GSReturn type;
    GSReturnMessagePayload payload;
};

typedef CircularFifo<GSMessage, 1024 * 1024 * 16> gs_fifo;
typedef CircularFifo<GSReturnMessage, 1024> gs_return_fifo;

class GraphicsSynthesizer
{
    private:
        INTC* intc;
        bool frame_complete;
        int frame_count;
        uint32_t* output_buffer1;
        uint32_t* output_buffer2;//double buffered to prevent mutex lock
        std::mutex output_buffer1_mutex, output_buffer2_mutex;
        bool using_first_buffer;
        std::unique_lock<std::mutex> current_lock;

        GS_REGISTERS reg;

        gs_fifo* message_queue;
        gs_return_fifo* return_queue;

        std::thread gsthread_id;
    public:
        GraphicsSynthesizer(INTC* intc);
        ~GraphicsSynthesizer();
        void send_message(GSMessage message);
        void reset();
        void start_frame();
        bool is_frame_complete();
        uint32_t* get_framebuffer();
        void render_CRT();
        uint32_t* render_partial_frame(uint16_t& width, uint16_t& height);
        void get_resolution(int& w, int& h);
        void get_inner_resolution(int& w, int& h);

        bool stalled();

        void set_VBLANK(bool is_VBLANK);
        void assert_FINISH();
        void assert_VSYNC();

        void set_CRT(bool interlaced, int mode, bool frame_mode);

        uint32_t read32_privileged(uint32_t addr);
        uint64_t read64_privileged(uint32_t addr);
        void write32_privileged(uint32_t addr, uint32_t value);
        void write64_privileged(uint32_t addr, uint64_t value);
        void write64(uint32_t addr, uint64_t value);

        void set_RGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a, float q);
        void set_ST(uint32_t s, uint32_t t);
        void set_UV(uint16_t u, uint16_t v);
        void set_XYZ(uint32_t x, uint32_t y, uint32_t z, bool drawing_kick);
        void set_XYZF(uint32_t x, uint32_t y, uint32_t z, uint8_t fog, bool drawing_kick);

        void load_state(std::ifstream& state);
        void save_state(std::ofstream& state);
        void send_dump_request();
        
};

inline bool GraphicsSynthesizer::stalled()
{
    return reg.CSR.SIGNAL_stall;
}

#endif // GS_HPP
