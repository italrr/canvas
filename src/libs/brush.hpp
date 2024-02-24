#ifndef CANVAS_STDLIB_BRUSH_HPP
    #define CANVAS_STDLIB_BRUSH_HPP

    #define SDL_MAIN_HANDLED

    #include <SDL2/SDL.h>
    #include <gl/GL.h>
    #include <chrono>

    #include "../cv.hpp"

    /*
        STANDARD LIBRARY `BRUSH`

        BRASH (canvas/cv) is a simple 2D drawing library that uses SDL2 and OpenGL 1.1 (or 1.0?) for
        rendering.
    
    */

   namespace brush {

        static SDL_Window* window = NULL;
        static bool isRunning = false;
        static unsigned int Sample2DPixel = 0;

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
                        cursor->setError("operator cv:window-creator: expects 5 arguments (STRING NUMBER NUMBER NUMBER NUMBER)");
                        return CV::create(0);
                    }

                    if(operands[0]->type != CV::ItemTypes::STRING){
                        cursor->setError("operator cv:window-creator: argument(1) must be a string");
                        return CV::create(0);
                    }

                    if(operands[1]->type != CV::ItemTypes::NUMBER || operands[2]->type != CV::ItemTypes::NUMBER || operands[3]->type != CV::ItemTypes::NUMBER || operands[4]->type != CV::ItemTypes::NUMBER){
                        cursor->setError("operator cv:window-creator: argument(1), argument(2), argument(3) and argument(4) must be a number");
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
                    return CV::create(1);
                }, {}
            )));


            lib->registerProperty("d-fillrect", std::make_shared<CV::FunctionType>(CV::FunctionType([lib](const std::vector<std::shared_ptr<CV::Item>> &operands, std::shared_ptr<CV::Item> &ctx, CV::Cursor *cursor){
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
                    // for(unsigned i=0; i< tnKeysn; i++){
                    //     KeysCheck[i] = 0;
                    //     isKeysStatePressed[i] = 0;
                    //     isKeysStateReleased[i] = 0;
                    // }

                    while (SDL_PollEvent(&event)){
                        switch(event.type){
                            case SDL_KEYUP:
                                // KeysState[getKey(event.key.keysym.sym)] = 0;
                            break;
                            case SDL_KEYDOWN:
                                // KeysCheck[getKey(event.key.keysym.sym)] = 1;
                                // KeysState[getKey(event.key.keysym.sym)] = 1;
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


            ctx->registerProperty("cv", std::static_pointer_cast<CV::Item>(lib));
        }   

    }


#endif