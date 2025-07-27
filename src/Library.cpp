#include "CV.hpp"

#if (_CV_PLATFORM == _CV_PLATFORM_TYPE_LINUX)
    #include <dlfcn.h> 
#elif  (_CV_PLATFORM == _CV_PLATFORM_TYPE_WINDOWS)
    // should prob move the entire DLL loading routines to its own file
    #include <windows.h>
#endif

static std::vector<void*> LoadedLibraries;

bool CV::ImportDynamicLibrary(const std::string &path, const std::string &fname, const std::shared_ptr<CV::Program> &prog, const CV::ContextType &ctx, const CV::CursorType &cursor){
    #if (_CV_PLATFORM == _CV_PLATFORM_TYPE_LINUX)
        void (*registerlibrary )(const std::shared_ptr<CV::Stack> &stack);
        void* handle = NULL;
        const char* error = NULL;
        handle = dlopen(path.c_str(), RTLD_LAZY);
        if(!handle){
            fprintf( stderr, "Failed to load dynamic library %s: %s\n", fname.c_str(), dlerror());
            std::exit(1);
        }
        dlerror();
        registerlibrary = (void (*)(const std::shared_ptr<CV::Stack> &stack)) dlsym( handle, "_CV_REGISTER_LIBRARY" );
        error = dlerror();
        if(error){
            fprintf( stderr, "Failed to load dynamic library '_CV_REGISTER_LIBRARY' from '%s': %s\n", fname.c_str(), error);
            dlclose(handle);
            std::exit(1);
        }
        // Try to load
        (*registerlibrary)(stack);
        loadedLibraries.push_back(handle);
        return true;
    #elif  (_CV_PLATFORM == _CV_PLATFORM_TYPE_WINDOWS)
        typedef void (*rlib)(const std::shared_ptr<CV::Program> &prog, const CV::ContextType &ctx, const CV::CursorType &cursor);

        // std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
        // std::wstring wide = converter.from_bytes(path);
        // const wchar_t *dllname = wide.c_str();
        rlib entry;
        HMODULE hdll;
        // Load lib
        hdll = LoadLibrary(path.c_str());   // error with VSC: according to VSC it expects wchar but according to the compiler, it expects char*
                                            // what? anyway. just ignore.
        if(hdll == NULL){
            fprintf( stderr, "Failed to load dynamic library %s\n", fname.c_str());
            std::exit(1);                
        }
        // Fetch register's address
        entry = (rlib)GetProcAddress(hdll, "_CV_REGISTER_LIBRARY");
        if(entry == NULL){
            fprintf( stderr, "Failed to load dynamic library '_CV_REGISTER_LIBRARY' from '%s'\n", fname.c_str());
            std::exit(1);      
        }
        entry(prog, ctx, cursor);
        LoadedLibraries.push_back(hdll);
        return true;
    #endif
}