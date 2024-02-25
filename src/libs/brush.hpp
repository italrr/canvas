#ifndef CANVAS_STDLIB_BRUSH_HPP
    #define CANVAS_STDLIB_BRUSH_HPP

    #define SDL_MAIN_HANDLED

    #include <SDL2/SDL.h>
    #include <gl/GL.h>
    #include <chrono>

    #include "../cv.hpp"

    /*
        STANDARD LIBRARY `BRUSH`

        BRUSH (named cv internally) is a simple 2D drawing library that uses SDL2 and OpenGL 1.1 (or 1.0?) for
        rendering simple 2D graphics.
    
    */

   namespace brush {

        static std::string LIBNAME = "cv";
        static SDL_Window* window = NULL;
        static bool isRunning = false;
        static unsigned int Sample2DPixel = 0;

        static const int keys[] = {
            SDLK_a,SDLK_b,SDLK_c,SDLK_d,SDLK_e,SDLK_f,SDLK_g,SDLK_h,SDLK_i,
            SDLK_j,SDLK_k,SDLK_l,SDLK_m,SDLK_n,SDLK_o,SDLK_p,SDLK_q,SDLK_r,
            SDLK_s,SDLK_t,SDLK_u,SDLK_v,SDLK_w,SDLK_x,SDLK_y,SDLK_z,SDLK_0,
            SDLK_1,SDLK_2,SDLK_3,SDLK_4,SDLK_5,SDLK_6,SDLK_7,SDLK_8,SDLK_9,
            SDLK_ESCAPE,SDLK_LCTRL,SDLK_LSHIFT,SDLK_LALT,SDLK_LGUI,SDLK_RCTRL,
            SDLK_RSHIFT,SDLK_RALT,SDLK_RGUI,SDLK_MENU,SDLK_LEFTBRACKET,
            SDLK_RIGHTBRACKET,SDLK_SEMICOLON,SDLK_COMMA,SDLK_PERIOD,SDLK_QUOTE,
            SDLK_SLASH,SDLK_BACKSLASH,SDLK_BACKQUOTE,SDLK_EQUALS,SDLK_SPACE,SDLK_SPACE,
            SDLK_RETURN,SDLK_BACKSPACE,SDLK_TAB,SDLK_PAGEUP,SDLK_PAGEDOWN,SDLK_END,
            SDLK_HOME,SDLK_INSERT,SDLK_DELETE,SDLK_PLUS,SDLK_MINUS,SDLK_ASTERISK,
            SDLK_SLASH,SDLK_LEFT,SDLK_RIGHT,SDLK_UP,SDLK_DOWN,SDLK_KP_0,SDLK_KP_1,
            SDLK_KP_2,SDLK_KP_3,SDLK_KP_4,SDLK_KP_5,SDLK_KP_6,SDLK_KP_7,SDLK_KP_8,
            SDLK_KP_9,SDLK_F1,SDLK_F2,SDLK_F3,SDLK_F4,SDLK_F5,SDLK_F6,SDLK_F7,
            SDLK_F8,SDLK_F9,SDLK_F10,SDLK_F11,SDLK_F12,SDLK_F13,SDLK_F14,SDLK_F15,
            SDLK_PAUSE
        };
        static const unsigned keysn = sizeof(keys)/sizeof(int);

        static std::unordered_map<unsigned, unsigned> SDLKeys;

        static unsigned getKey(unsigned key){
            return SDLKeys[key];
        }

        static bool KeysState [keysn];
        static bool KeysCheck [keysn];
        static bool KeysStatePressed [keysn];
        static bool isKeysStatePressed [keysn];
        static bool KeysStateReleased [keysn];
        static bool isKeysStateReleased [keysn];        

        static void registerLibrary(std::shared_ptr<CV::Item> &ctx){
            auto lib = std::make_shared<CV::ProtoType>(CV::ProtoType());

            lib->registerProperty("title", CV::createString("CANVAS APPLICATION"));
            lib->registerProperty("width", CV::create(0));
            lib->registerProperty("height", CV::create(0));
            lib->registerProperty("x-scale", CV::create(0));
            lib->registerProperty("y-scale", CV::create(0));            
            lib->registerProperty("running", CV::create(0));

            lib->registerProperty("window-create", std::make_shared<CV::FunctionType>(CV::FunctionType([lib](const std::vector<std::shared_ptr<CV::Item>> &operands, std::shared_ptr<CV::Item> &ctx, CV::Cursor *cursor){

                    if(operands.size() < 5){
                        cursor->setError("operator "+LIBNAME+":window-creator: expects 5 arguments (STRING NUMBER NUMBER NUMBER NUMBER)");
                        return CV::create(0);
                    }

                    if(operands[0]->type != CV::ItemTypes::STRING){
                        cursor->setError("operator "+LIBNAME+":window-creator: argument(1) must be a string");
                        return CV::create(0);
                    }

                    if(operands[1]->type != CV::ItemTypes::NUMBER || operands[2]->type != CV::ItemTypes::NUMBER || operands[3]->type != CV::ItemTypes::NUMBER || operands[4]->type != CV::ItemTypes::NUMBER){
                        cursor->setError("operator "+LIBNAME+":window-creator: argument(1), argument(2), argument(3) and argument(4) must be a number");
                        return CV::create(0);

                    }



                    std::string title = std::static_pointer_cast<CV::StringType>(operands[0])->literal;
                    auto xScale = *static_cast<double*>(operands[3]->data);
                    auto yScale = *static_cast<double*>(operands[4]->data);
                    auto width = *static_cast<double*>(operands[1]->data);
                    auto height = *static_cast<double*>(operands[2]->data);                    

                    std::static_pointer_cast<CV::StringType>(lib->getProperty("title"))->set(title);

                    lib->getProperty("width")->write(&width, sizeof(width), CV::ItemTypes::NUMBER);
                    lib->getProperty("height")->write(&height, sizeof(height), CV::ItemTypes::NUMBER);
                    lib->getProperty("x-scale")->write(&xScale, sizeof(xScale), CV::ItemTypes::NUMBER);
                    lib->getProperty("y-scale")->write(&yScale, sizeof(yScale), CV::ItemTypes::NUMBER);                    


                    if(SDL_Init(SDL_INIT_VIDEO) < 0){
                        cursor->setError("operator brush:window-creator: Failed to start SDL2");
                        return CV::create(0);
                    }

                    window = SDL_CreateWindow(
                        title.c_str(),
                        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                        width * xScale,
                        height * yScale,
                        SDL_WINDOW_OPENGL
                    );                    

                    if(window == NULL){
                        cursor->setError("operator brush:window-creator: SDL2 Failed to create Window "+std::string()+SDL_GetError());
                        return CV::create(0);
                    }

                    SDL_GL_CreateContext(window);

                    unsigned char *dummyTexture = new unsigned char[8 * 8 * 4];

                    for(unsigned i = 0; i < 8*8*4; i += 4){
                        dummyTexture[i] = 255;
                        dummyTexture[i + 1] = 255;
                        dummyTexture[i + 2] = 255;
                        dummyTexture[i + 3] = 255;
                    }

                    glEnable( GL_TEXTURE_2D );
                    glGenTextures( 1, &Sample2DPixel );
                    glBindTexture( GL_TEXTURE_2D, Sample2DPixel );
                    glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, 8, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE, dummyTexture );
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);		
                    glBindTexture( GL_TEXTURE_2D, 0);

                    delete dummyTexture;

                    double r = 1;
                    lib->getProperty("running")->write(&r, sizeof(r), true);
                    isRunning = true;

                    return CV::create(1);
                }, {}
            )));

            lib->registerProperty("window-close", std::make_shared<CV::FunctionType>(CV::FunctionType([lib](const std::vector<std::shared_ptr<CV::Item>> &operands, std::shared_ptr<CV::Item> &ctx, CV::Cursor *cursor){
                    double r = 0;
                    lib->getProperty("running")->write(&r, sizeof(r), true);
                    isRunning = false;
                    SDL_DestroyWindow(window);
                    SDL_Quit();
                    window = NULL;
                    return CV::create(1);
                }, {}
            )));


            lib->registerProperty("d-rectangle", std::make_shared<CV::FunctionType>(CV::FunctionType([lib](const std::vector<std::shared_ptr<CV::Item>> &operands, std::shared_ptr<CV::Item> &ctx, CV::Cursor *cursor){
                    if(operands.size() < 6){
                        cursor->setError("operator brush:d-rect: expects 6 arguments (NUMBER NUMBER NUMBER NUMBER NUMBER LIST)");
                        return CV::create(0);
                    }

                    if(operands[0]->type != CV::ItemTypes::NUMBER){
                        cursor->setError("operator brush:d-rect: argument(1) must be a NUMBER");
                        return CV::create(0);

                    }

                    if(operands[1]->type != CV::ItemTypes::NUMBER){
                        cursor->setError("operator brush:d-rect: argument(2) must be a NUMBER");
                        return CV::create(0);
                    }

                    if(operands[2]->type != CV::ItemTypes::NUMBER){
                        cursor->setError("operator brush:d-rect: argument(3) must be a NUMBER");
                        return CV::create(0);

                    }

                    if(operands[3]->type != CV::ItemTypes::NUMBER){
                        cursor->setError("operator brush:d-rect: argument(4) must be a NUMBER");
                        return CV::create(0);
                    }            

                    if(operands[4]->type != CV::ItemTypes::NUMBER){
                        cursor->setError("operator brush:d-rect: argument(5) must be a NUMBER");
                        return CV::create(0);
                    }                    


                    if(operands[5]->type != CV::ItemTypes::LIST && std::static_pointer_cast<CV::ListType>(operands[5])->list.size() < 4){
                        cursor->setError("operator brush:d-rect: argument(6) must be a LIST with 4 componenets (R G B A)");
                        return CV::create(0);

                    }                    


                    double _x = *static_cast<double*>(operands[0]->data);
                    double _y = *static_cast<double*>(operands[1]->data);
                    double _w = *static_cast<double*>(operands[2]->data);
                    double _h = *static_cast<double*>(operands[3]->data);
                    int fill = (int)*static_cast<double*>(operands[4]->data);

                    std::shared_ptr<CV::ListType> color = std::static_pointer_cast<CV::ListType>(operands[5]);

                    double rgba[4] = {
                        *static_cast<double*>(color->list[0]->data),
                        *static_cast<double*>(color->list[1]->data),
                        *static_cast<double*>(color->list[2]->data),
                        *static_cast<double*>(color->list[3]->data)
                    };


                    int width = *static_cast<double*>(lib->getProperty("width")->data);
                    int height = *static_cast<double*>(lib->getProperty("height")->data);
                    int xScale = *static_cast<double*>(lib->getProperty("x-scale")->data);
                    int yScale = *static_cast<double*>(lib->getProperty("y-scale")->data);                    

                    glEnable(GL_BLEND);
                    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

                    glBindTexture(GL_TEXTURE_2D, Sample2DPixel);
                    
                    glColor4f(rgba[0], rgba[1], rgba[2], rgba[3]);

                    int x = _x * xScale;
                    int y = _y * yScale;

                    int w = _w * xScale;
                    int h = _h * yScale;

                    if(!fill){
                        glLineWidth(4);
                    }
                    glBegin(fill ? GL_QUADS : GL_LINE_LOOP);
                        glTexCoord2f(0, 0);
                        glVertex2f(x, y);
                        glTexCoord2f(1, 0);
                        glVertex2f(x + w, y);
                        glTexCoord2f( 1, 1 );
                        glVertex2f(x + w, y + h);
                        glTexCoord2f( 0, 1 );
                        glVertex2f( x, y + h);
                    glEnd();


                    return CV::create(1);
                }, {}
            )));


            lib->registerProperty("d-pixel", std::make_shared<CV::FunctionType>(CV::FunctionType([lib](const std::vector<std::shared_ptr<CV::Item>> &operands, std::shared_ptr<CV::Item> &ctx, CV::Cursor *cursor){

                    if(operands.size() < 3){
                        cursor->setError("operator brush:window-creator: expects 3 arguments (NUMBER NUMBER LIST)");
                        return CV::create(0);
                    }

                    if(operands[0]->type != CV::ItemTypes::NUMBER){
                        cursor->setError("operator brush:window-creator: argument(1) must be a NUMBER");
                        return CV::create(0);

                    }

                    if(operands[1]->type != CV::ItemTypes::NUMBER){
                        cursor->setError("operator brush:window-creator: argument(2) must be a NUMBER");
                        return CV::create(0);

                    }

                    if(operands[2]->type != CV::ItemTypes::LIST && std::static_pointer_cast<CV::ListType>(operands[2])->list.size() < 4){
                        cursor->setError("operator brush:window-creator: argument(2) must be a LIST with 4 componenets (R G B A)");
                        return CV::create(0);

                    }                    


                    double _x = *static_cast<double*>(operands[0]->data);
                    double _y = *static_cast<double*>(operands[1]->data);
                    std::shared_ptr<CV::ListType> color = std::static_pointer_cast<CV::ListType>(operands[2]);

                    double rgba[4] = {
                        *static_cast<double*>(color->list[0]->data),
                        *static_cast<double*>(color->list[1]->data),
                        *static_cast<double*>(color->list[2]->data),
                        *static_cast<double*>(color->list[3]->data)
                    };


                    int width = *static_cast<double*>(lib->getProperty("width")->data);
                    int height = *static_cast<double*>(lib->getProperty("height")->data);
                    int xScale = *static_cast<double*>(lib->getProperty("x-scale")->data);
                    int yScale = *static_cast<double*>(lib->getProperty("y-scale")->data);                    

                    glEnable(GL_BLEND);
                    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

                    glBindTexture(GL_TEXTURE_2D, Sample2DPixel);
                    
                    glColor4f(rgba[0], rgba[1], rgba[2], rgba[3]);

                    if(_x < 0 || _y < 0 || _x > width || _y > height){
                        return CV::create(0);
                    }

                    int x = _x * xScale;
                    int y = _y * yScale;



                    glBegin(GL_QUADS);
                        glTexCoord2f(0, 0);
                        glVertex2f(x, y);
                        glTexCoord2f(1, 0);
                        glVertex2f(x + xScale, y);
                        glTexCoord2f( 1, 1 );
                        glVertex2f(x + xScale, y + yScale);
                        glTexCoord2f( 0, 1 );
                        glVertex2f( x, y + yScale);
                    glEnd();


                    return CV::create(1);
                }, {}
            )));

           lib->registerProperty("d-fillrect", std::make_shared<CV::FunctionType>(CV::FunctionType([lib](const std::vector<std::shared_ptr<CV::Item>> &operands, std::shared_ptr<CV::Item> &ctx, CV::Cursor *cursor){
                    return CV::create(1);
                }, {}
            )));

           lib->registerProperty("d-rect", std::make_shared<CV::FunctionType>(CV::FunctionType([lib](const std::vector<std::shared_ptr<CV::Item>> &operands, std::shared_ptr<CV::Item> &ctx, CV::Cursor *cursor){
                    return CV::create(1);
                }, {}
            )));     

           lib->registerProperty("clear", std::make_shared<CV::FunctionType>(CV::FunctionType([lib](const std::vector<std::shared_ptr<CV::Item>> &operands, std::shared_ptr<CV::Item> &ctx, CV::Cursor *cursor){
                    int xScale = *static_cast<double*>(lib->getProperty("x-scale")->data);
                    int yScale = *static_cast<double*>(lib->getProperty("y-scale")->data);                  
                    int width = *static_cast<double*>(lib->getProperty("width")->data);
                    int height = *static_cast<double*>(lib->getProperty("height")->data);
                    glClear(GL_COLOR_BUFFER_BIT);
                    glClearColor(0, 0, 0, 0);
                    glViewport(0, 0, width * xScale, height * yScale);


                    glMatrixMode(GL_PROJECTION);
                    glLoadIdentity();
                    glOrtho(0, width * xScale, height * yScale, 0, 1, -1);
                    glMatrixMode(GL_MODELVIEW);
                    glLoadIdentity(); 
                    return CV::create(1);
                }, {}
            )));  

           lib->registerProperty("step", std::make_shared<CV::FunctionType>(CV::FunctionType([lib](const std::vector<std::shared_ptr<CV::Item>> &operands, std::shared_ptr<CV::Item> &ctx, CV::Cursor *cursor){

                    SDL_Event event;
                    unsigned key;
                    for(unsigned i=0; i< keysn; i++){
                        KeysCheck[i] = 0;
                        isKeysStatePressed[i] = 0;
                        isKeysStateReleased[i] = 0;
                    }

                    while (SDL_PollEvent(&event)){
                        switch(event.type){
                            case SDL_KEYUP:
                                KeysState[getKey(event.key.keysym.sym)] = 0;
                            break;
                            case SDL_KEYDOWN:
                                KeysCheck[getKey(event.key.keysym.sym)] = 1;
                                KeysState[getKey(event.key.keysym.sym)] = 1;
                            break;
                            case SDL_MOUSEBUTTONDOWN:
                                // buttonState[getButton(event.button.button)] = 1;
                            break;
                            case SDL_MOUSEBUTTONUP:
                                // buttonState[getButton(event.button.button)] = 0;
                            break;
                            case SDL_QUIT:
                                double r = 0;
                                lib->getProperty("running")->write(&r, sizeof(r), true);
                                isRunning = false;
                            break;
                        }

                    }

                    return CV::create(1);
                }, {}
            )));    

            lib->registerProperty("draw", std::make_shared<CV::FunctionType>(CV::FunctionType([lib](const std::vector<std::shared_ptr<CV::Item>> &operands, std::shared_ptr<CV::Item> &ctx, CV::Cursor *cursor){
                    SDL_GL_SwapWindow(window);
                    glClear(GL_COLOR_BUFFER_BIT);
                    glClearColor(0.7f, 0.7f, 0.7f, 1.0f);              
                    return CV::create(1);
                }, {}
            )));     

            lib->registerProperty("kb-check", std::make_shared<CV::FunctionType>(CV::FunctionType([lib](const std::vector<std::shared_ptr<CV::Item>> &operands, std::shared_ptr<CV::Item> &ctx, CV::Cursor *cursor){

                    if(operands.size() < 1){
                        cursor->setError("operator "+LIBNAME+":kb-check: expects 1 argument");
                        return CV::create(0);
                    }

                    
                    if(operands[0]->type != CV::ItemTypes::NUMBER){
                        cursor->setError("operator brush:window-creator: argument(1) must be a NUMBER");
                        return CV::create(0);
                    }                    

                    int key = (int)*static_cast<double*>(operands[0]->data); // cast it to int

                    return CV::create((key >= keysn-1 ? 0 : KeysCheck[key]));
                }, {}
            )));    


            lib->registerProperty("kb-press-once", std::make_shared<CV::FunctionType>(CV::FunctionType([lib](const std::vector<std::shared_ptr<CV::Item>> &operands, std::shared_ptr<CV::Item> &ctx, CV::Cursor *cursor){

                    if(operands.size() < 1){
                        cursor->setError("operator "+LIBNAME+":kb-press-once: expects 1 argument");
                        return CV::create(0);
                    }

                    
                    if(operands[0]->type != CV::ItemTypes::NUMBER){
                        cursor->setError("operator "+LIBNAME+":kb-press-once: argument(1) must be a NUMBER");
                        return CV::create(0);
                    }                    

                    int key = (int)*static_cast<double*>(operands[0]->data); // cast it to int

                    if (key >= keysn-1) return CV::create(0);
                    bool b = KeysCheck[key];
                    KeysState[key] = 0;
                    KeysStatePressed[key] = 0;
                    isKeysStatePressed[key] = 0;
                    KeysCheck[key] = 0;
                    return CV::create(b);
                }, {}
            )));     


            lib->registerProperty("kb-pressed", std::make_shared<CV::FunctionType>(CV::FunctionType([lib](const std::vector<std::shared_ptr<CV::Item>> &operands, std::shared_ptr<CV::Item> &ctx, CV::Cursor *cursor){

                    if(operands.size() < 1){
                        cursor->setError("operator "+LIBNAME+":kb-pressed: expects 1 argument");
                        return CV::create(0);
                    }

                    
                    if(operands[0]->type != CV::ItemTypes::NUMBER){
                        cursor->setError("operator "+LIBNAME+":kb-pressed: argument(1) must be a NUMBER");
                        return CV::create(0);
                    }                    

                    int key = (int)*static_cast<double*>(operands[0]->data); // cast it to int

                    if (key >= keysn){
                        return CV::create(0);
                    }
                    if (isKeysStatePressed[key]){
                        return CV::create(1);
                    }
                    if (KeysStatePressed[key] && KeysState[key]){
                        return CV::create(0);
                    }
                    if (KeysStatePressed[key]){
                        KeysStatePressed[key] = 0;
                        return CV::create(0);
                    }
                    if (!KeysStatePressed[key] && KeysState[key]){
                        isKeysStatePressed[key] = 1;
                        KeysStatePressed[key] = 1;
                        return CV::create(1);
                    }
                    return CV::create(0);
                }, {}
            )));                                                  

            lib->registerProperty("kb-released", std::make_shared<CV::FunctionType>(CV::FunctionType([lib](const std::vector<std::shared_ptr<CV::Item>> &operands, std::shared_ptr<CV::Item> &ctx, CV::Cursor *cursor){

                    if(operands.size() < 1){
                        cursor->setError("operator "+LIBNAME+":kb-released: expects 1 argument");
                        return CV::create(0);
                    }

                    if(operands[0]->type != CV::ItemTypes::NUMBER){
                        cursor->setError("operator "+LIBNAME+":kb-released: argument(1) must be a NUMBER");
                        return CV::create(0);
                    }                    

                    int key = (int)*static_cast<double*>(operands[0]->data); // cast it to int

                    if (key >= keysn){
                        return CV::create(0);
                    }
                    if (isKeysStateReleased[key]){
                        return CV::create(1);
                    }
                    if (KeysState[key]){
                        KeysStateReleased[key] = 1;
                        return CV::create(0);
                    }
                    if (!KeysState[key] && KeysStateReleased[key]){
                        KeysStateReleased[key] = 0;
                        isKeysStateReleased[key] = 1;
                        return CV::create(1);
                    }

                    return CV::create(0);
                }, {}
            )));                                                  


            // register keyboard keys
            std::vector<std::string> defaultKeys = {
                "key-A", "key-B", "key-C", "key-D", "key-E", "key-F", "key-G", "key-H", "key-I", 
                "key-J", "key-K", "key-L", "key-M", "key-N", "key-O", "key-P", "key-Q", "key-R", 
                "key-S", "key-T", "key-U", "key-V", "key-W", "key-X", "key-Y", "key-Z", "key-0", 
                "key-1", "key-2", "key-3", "key-4", "key-5", "key-6", "key-7", "key-8", "key-9", 
                "key-ESCAPE", "key-LCONTROL", "key-LSHIFT", "key-LALT", "key-LSYSTEM", "key-RCONTROL", 
                "key-RSHIFT", "key-RALT", "key-RSYSTEM", "key-MENU", "key-LBRACKET", "key-RBRACKET", 
                "key-SEMICOLON", "key-COMMA", "key-PERIOD", "key-QUOTE", "key-SLASH", "key-BACKSLASH", 
                "key-TILDE", "key-EQUAL", "key-DASH", "key-SPACE", "key-ENTER", "key-BACK", "key-TAB", 
                "key-PAGEUP", "key-PAGEDOWN", "key-END", "key-HOME", "key-INSERT", "key-DELETE", "key-ADD", 
                "key-SUBTRACT", "key-MULTIPLY", "key-DIVIDE", "key-LEFT", "key-RIGHT", "key-UP", "key-DOWN", 
                "key-NUMPAD0", "key-NUMPAD1", "key-NUMPAD2", "key-NUMPAD3", "key-NUMPAD4", "key-NUMPAD5", 
                "key-NUMPAD6", "key-NUMPAD7", "key-NUMPAD8", "key-NUMPAD9", "key-F1", "key-F2", "key-F3", 
                "key-F4", "key-F5", "key-F6", "key-F7", "key-F8", "key-F9", "key-F10", "key-F11", "key-F12", 
                "key-F13", "key-F14", "key-F15", "key-PAUSE"                
            };
            for(int i = 0; i < defaultKeys.size(); ++i){
                lib->registerProperty(defaultKeys[i], CV::create(i));    
            }

            ctx->registerProperty(LIBNAME, std::static_pointer_cast<CV::Item>(lib));
        }   

    }


#endif