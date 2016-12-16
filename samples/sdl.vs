// SDL
// ./verse samples/sdl.vs -lSDL2

type SDL_Window : struct {
    _ : bool;
};
type SDL_Renderer : struct {
    _ : bool;
};

enum WindowFlags {
    SDL_WINDOW_OPENGL = 2, 
};

enum SDL_WindowEventID 
{
    SDL_WINDOWEVENT_NONE,           
    SDL_WINDOWEVENT_SHOWN,          
    SDL_WINDOWEVENT_HIDDEN,         
    SDL_WINDOWEVENT_EXPOSED,        
    SDL_WINDOWEVENT_MOVED,          
    SDL_WINDOWEVENT_RESIZED,        
    SDL_WINDOWEVENT_SIZE_CHANGED,   
    SDL_WINDOWEVENT_MINIMIZED,      
    SDL_WINDOWEVENT_MAXIMIZED,      
    SDL_WINDOWEVENT_RESTORED,       
    SDL_WINDOWEVENT_ENTER,          
    SDL_WINDOWEVENT_LEAVE,          
    SDL_WINDOWEVENT_FOCUS_GAINED,   
    SDL_WINDOWEVENT_FOCUS_LOST,     
    SDL_WINDOWEVENT_CLOSE           
};

enum SDL_GLattr
{
    SDL_GL_RED_SIZE,
    SDL_GL_GREEN_SIZE,
    SDL_GL_BLUE_SIZE,
    SDL_GL_ALPHA_SIZE,
    SDL_GL_BUFFER_SIZE,
    SDL_GL_DOUBLEBUFFER,
    SDL_GL_DEPTH_SIZE,
    SDL_GL_STENCIL_SIZE,
    SDL_GL_ACCUM_RED_SIZE,
    SDL_GL_ACCUM_GREEN_SIZE,
    SDL_GL_ACCUM_BLUE_SIZE,
    SDL_GL_ACCUM_ALPHA_SIZE,
    SDL_GL_STEREO,
    SDL_GL_MULTISAMPLEBUFFERS,
    SDL_GL_MULTISAMPLESAMPLES,
    SDL_GL_ACCELERATED_VISUAL,
    SDL_GL_RETAINED_BACKING,
    SDL_GL_CONTEXT_MAJOR_VERSION,
    SDL_GL_CONTEXT_MINOR_VERSION,
    SDL_GL_CONTEXT_EGL,
    SDL_GL_CONTEXT_FLAGS,
    SDL_GL_CONTEXT_PROFILE_MASK,
    SDL_GL_SHARE_WITH_CURRENT_CONTEXT,
    SDL_GL_FRAMEBUFFER_SRGB_CAPABLE,
    SDL_GL_CONTEXT_RELEASE_BEHAVIOR
};

SDL_INIT_VIDEO:u32 = 0x20;

extern fn SDL_Init(u32):int;
extern fn SDL_CreateRenderer(&SDL_Window, int, int):&SDL_Renderer;
extern fn SDL_CreateWindow(&u8, int, int, int, int, WindowFlags):&SDL_Window;
extern fn SDL_GL_SetAttribute(SDL_GLattr, int);
extern fn SDL_SetRenderDrawColor(&SDL_Renderer, int, int, int, int);
extern fn SDL_RenderClear(&SDL_Renderer);

extern fn SDL_GL_SetSwapInterval(int);
extern fn SDL_DestroyWindow(&SDL_Window);
extern fn SDL_Quit();
extern fn SDL_GetPerformanceCounter():int;
extern fn SDL_GetPerformanceFrequency():float;
extern fn SDL_PollEvent(&SDL_Event):bool;
extern fn SDL_GL_SwapWindow(&SDL_Window);
extern fn SDL_Delay(int);

enum SDL_EventType {
   SDL_NOEVENT = 0,	   
   SDL_ACTIVEEVENT,
   SDL_KEYDOWN,		   
   SDL_KEYUP,		   
   SDL_MOUSEMOTION,	   
   SDL_MOUSEBUTTONDOWN,
   SDL_MOUSEBUTTONUP,  
   SDL_JOYAXISMOTION,  
   SDL_JOYBALLMOTION, 
   SDL_JOYHATMOTION,
   SDL_JOYBUTTONDOWN, 
   SDL_JOYBUTTONUP,	  
   SDL_QUIT,		  
   SDL_SYSWMEVENT,	   
   SDL_EVENT_RESERVEDA,
   SDL_EVENT_RESERVEDB,
   SDL_VIDEORESIZE,	   
   SDL_VIDEOEXPOSE,	   
   SDL_EVENT_RESERVED2,
   SDL_EVENT_RESERVED3,
   SDL_EVENT_RESERVED4,
   SDL_EVENT_RESERVED5,
   SDL_EVENT_RESERVED6,
   SDL_EVENT_RESERVED7,
   SDL_USEREVENT = 24,
   SDL_NUMEVENTS = 32
};

type SDL_Event : struct {
    Type:SDL_EventType;
};

window:&SDL_Window;
renderer:&SDL_Renderer;
running := true;

fn start():bool {
    if SDL_Init(SDL_INIT_VIDEO) < 0 {
        println("Error!");// + SDL_GetError());
        return false;
    }
    use SDL_GLattr;
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);

    window = SDL_CreateWindow("Hello SDL".bytes, 10, 10, 640, 480, WindowFlags.SDL_WINDOW_OPENGL);

    if !validptr(window as ptr) {
        println("UH OH");
        return false;
    }

    renderer = SDL_CreateRenderer(window, -1, 0);
    SDL_GL_SetSwapInterval(1);
    
    running = true;
    return running;
}

fn end() {
    SDL_DestroyWindow(window);
    SDL_Quit();
}

fn loop() {
    event:SDL_Event;
    old_ticks:int;
    accumulator:float;

    while running {
        now := SDL_GetPerformanceCounter();
        dt := (now - old_ticks) / (SDL_GetPerformanceFrequency() as float);

        old_ticks = now;

        if dt > 0.1 {
            dt = 0.01;
        }

        while SDL_PollEvent(&event) {
            handle_event(&event);
        }
        accumulator += dt;
        while accumulator >= 0.01 {
            update(0.01);
            accumulator -= 0.01;
        }

        draw();
        SDL_GL_SwapWindow(window);

        SDL_Delay(1);
    }
}

fn update(dt:float) {
}

fn handle_event(event:&SDL_Event) {
    use SDL_EventType;
    if event.Type == SDL_QUIT {
        stop();
    }
}

fn draw() {
    SDL_SetRenderDrawColor(renderer, 0xff, 0, 0, 0xff);
    SDL_RenderClear(renderer);
}

fn stop() {
    running = false;
}

fn main():int {
    if !start() {
        return 1;
    }
    loop();
    return 0;
}
