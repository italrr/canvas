#include <cstdio>
#include <filesystem>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <system_error>

#if defined(_WIN32)
    #include <windows.h>
#elif defined(__APPLE__) || defined(__linux__)
    #include <sys/stat.h>
#endif

#include "../CV.hpp"

namespace {
    static int GEN_ID(){
        static std::mutex access;
        static int v = 1000;
        access.lock();
        int r = ++v;
        access.unlock();
        return r;
    }

    static std::shared_ptr<CV::Data> __cv_file_unwrap(const std::shared_ptr<CV::Data> &d){
        return d ? d->unwrap() : std::shared_ptr<CV::Data>(nullptr);
    }

    static bool __cv_file_expect_exactly(
        const std::string &name,
        const std::vector<std::pair<std::string, std::shared_ptr<CV::Data>>> &args,
        int n,
        const CV::CursorType &cursor,
        const CV::TokenType &token
    ){
        if(static_cast<int>(args.size()) != n){
            cursor->setError(
                CV_ERROR_MSG_MISUSED_FUNCTION,
                "'"+name+"' expects exactly ("+std::to_string(n)+") argument(s)",
                token
            );
            return false;
        }
        return true;
    }

    static bool __cv_file_expect_type(
        const std::string &name,
        const std::shared_ptr<CV::Data> &value,
        CV::DataType expected,
        const CV::CursorType &cursor,
        const CV::TokenType &token
    ){
        auto v = __cv_file_unwrap(value);
        if(!v || v->type != expected){
            cursor->setError(
                CV_ERROR_MSG_WRONG_OPERANDS,
                "Function '"+name+"' expected "+CV::DataTypeName(expected)+
                ", provided "+(v ? CV::DataTypeName(v->type) : std::string("NULL")),
                token
            );
            return false;
        }
        return true;
    }


    static bool __cv_file_extract_path_string(
        const std::string &fname,
        const std::vector<std::pair<std::string, std::shared_ptr<CV::Data>>> &args,
        std::filesystem::path &outPath,
        const CV::CursorType &cursor,
        const CV::TokenType &token
    ){
        if(!__cv_file_expect_exactly(fname, args, 1, cursor, token)){
            return false;
        }

        auto pathData = __cv_file_unwrap(args[0].second);
        if(!__cv_file_expect_type(fname, pathData, CV::DataType::STRING, cursor, token)){
            return false;
        }

        auto raw = std::static_pointer_cast<CV::DataString>(pathData)->v;

        try{
            outPath = std::filesystem::path(raw);
        }catch(...){
            cursor->setError(
                CV_ERROR_MSG_WRONG_OPERANDS,
                "Function '"+fname+"' was given an invalid path '"+raw+"'",
                token
            );
            return false;
        }

        return true;
    }

    static CV_NUMBER __cv_file_filetime_to_epoch_seconds(std::filesystem::file_time_type ft){
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            ft - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now()
        );

        return static_cast<CV_NUMBER>(
            std::chrono::duration_cast<std::chrono::seconds>(sctp.time_since_epoch()).count()
        );
    }

    static bool __cv_file_get_created_at_seconds(
        const std::filesystem::path &path,
        CV_NUMBER &outEpoch,
        const CV::CursorType &cursor,
        const CV::TokenType &token,
        const std::string &fname
    ){
    #if defined(_WIN32)
        WIN32_FILE_ATTRIBUTE_DATA data;
        if(!GetFileAttributesExA(path.string().c_str(), GetFileExInfoStandard, &data)){
            cursor->setError(
                CV_ERROR_MSG_WRONG_OPERANDS,
                "Function '"+fname+"' failed to query file creation time",
                token
            );
            return false;
        }

        ULARGE_INTEGER ull;
        ull.LowPart = data.ftCreationTime.dwLowDateTime;
        ull.HighPart = data.ftCreationTime.dwHighDateTime;

        // Windows FILETIME is 100ns intervals since 1601-01-01 UTC
        const unsigned long long EPOCH_DIFF_100NS = 116444736000000000ULL;
        if(ull.QuadPart < EPOCH_DIFF_100NS){
            outEpoch = 0;
        }else{
            outEpoch = static_cast<CV_NUMBER>((ull.QuadPart - EPOCH_DIFF_100NS) / 10000000ULL);
        }
        return true;

    #elif defined(__APPLE__)
        struct stat st{};
        if(stat(path.string().c_str(), &st) != 0){
            cursor->setError(
                CV_ERROR_MSG_WRONG_OPERANDS,
                "Function '"+fname+"' failed to query file creation time",
                token
            );
            return false;
        }

        outEpoch = static_cast<CV_NUMBER>(st.st_birthtimespec.tv_sec);
        return true;

    #elif defined(__linux__)
        struct stat st{};
        if(stat(path.string().c_str(), &st) != 0){
            cursor->setError(
                CV_ERROR_MSG_WRONG_OPERANDS,
                "Function '"+fname+"' failed to query file creation time",
                token
            );
            return false;
        }

        // Linux often does not expose true birth/creation time portably here.
        // Fallback: metadata change time.
        outEpoch = static_cast<CV_NUMBER>(st.st_ctime);
        return true;

    #else
        cursor->setError(
            CV_ERROR_MSG_WRONG_OPERANDS,
            "Function '"+fname+"' does not support creation-time lookup on this platform",
            token
        );
        return false;
    #endif
    }

    struct __cv_file_entry {
        FILE *fp = nullptr;
        std::string filename;
        std::string path;
        std::string file_path;
        std::string extension;
        std::string mode;
    };

    static std::mutex __cv_file_mutex;
    static std::unordered_map<int, __cv_file_entry> __cv_file_handles;

    static bool __cv_file_parse_mode(
        const std::string &rawMode,
        std::string &normalized,
        const CV::CursorType &cursor,
        const CV::TokenType &token,
        const std::string &fname
    ){
        auto mode = CV::Tools::upper(rawMode);

        if(mode == "ASCII" || mode == "BINARY" || mode == "UTF-8"){
            normalized = mode;
            return true;
        }

        cursor->setError(
            CV_ERROR_MSG_WRONG_OPERANDS,
            "Function '"+fname+"' expects mode to be 'ASCII', 'BINARY', or 'UTF-8'",
            token
        );
        return false;
    }

    static std::shared_ptr<CV::DataStore> __cv_file_make_descriptor(
        const CV::ContextType &ctx,
        int id,
        const __cv_file_entry &entry
    ){
        auto out = ctx->buildStore();

        out->v["fileid"] = std::static_pointer_cast<CV::Data>(ctx->buildNumber(id));
        out->v["filename"] = std::static_pointer_cast<CV::Data>(ctx->buildString(entry.filename));
        out->v["path"] = std::static_pointer_cast<CV::Data>(ctx->buildString(entry.path));
        out->v["file_path"] = std::static_pointer_cast<CV::Data>(ctx->buildString(entry.file_path));
        out->v["extension"] = std::static_pointer_cast<CV::Data>(ctx->buildString(entry.extension));
        out->v["mode"] = std::static_pointer_cast<CV::Data>(ctx->buildString(entry.mode));

        return out;
    }

    static bool __cv_file_extract_handle(
        const std::string &fname,
        const std::shared_ptr<CV::Data> &subject,
        int &fileId,
        __cv_file_entry &entry,
        const CV::CursorType &cursor,
        const CV::TokenType &token
    ){
        auto v = __cv_file_unwrap(subject);
        if(!v || v->type != CV::DataType::STORE){
            cursor->setError(
                CV_ERROR_MSG_WRONG_OPERANDS,
                "Function '"+fname+"' expects a file STORE",
                token
            );
            return false;
        }

        auto store = std::static_pointer_cast<CV::DataStore>(v);

        if(store->v.count("fileid") == 0){
            cursor->setError(
                CV_ERROR_MSG_WRONG_OPERANDS,
                "Function '"+fname+"' expects store field 'fileid'",
                token
            );
            return false;
        }

        auto fileIdData = __cv_file_unwrap(store->v["fileid"]);
        if(!fileIdData || fileIdData->type != CV::DataType::NUMBER){
            cursor->setError(
                CV_ERROR_MSG_WRONG_OPERANDS,
                "Function '"+fname+"' expects store field 'fileid' to be NUMBER",
                token
            );
            return false;
        }

        fileId = static_cast<int>(std::static_pointer_cast<CV::DataNumber>(fileIdData)->v);

        std::lock_guard<std::mutex> lock(__cv_file_mutex);
        if(__cv_file_handles.count(fileId) == 0 || __cv_file_handles[fileId].fp == nullptr){
            cursor->setError(
                CV_ERROR_MSG_WRONG_OPERANDS,
                "Function '"+fname+"' was given a closed or invalid file descriptor",
                token
            );
            return false;
        }

        entry = __cv_file_handles[fileId];
        return true;
    }

    static bool __cv_file_update_handle(
        int fileId,
        const __cv_file_entry &entry
    ){
        std::lock_guard<std::mutex> lock(__cv_file_mutex);
        if(__cv_file_handles.count(fileId) == 0){
            return false;
        }
        __cv_file_handles[fileId] = entry;
        return true;
    }

    static std::vector<unsigned char> __cv_file_bits_to_bytes(
        const std::vector<std::pair<std::string, std::shared_ptr<CV::Data>>> &bitArgs,
        const CV::CursorType &cursor,
        const CV::TokenType &token,
        const std::string &fname
    ){
        std::vector<unsigned char> out;

        if(bitArgs.empty()){
            return out;
        }

        auto only = __cv_file_unwrap(bitArgs[0].second);
        if(!only || only->type != CV::DataType::LIST){
            cursor->setError(
                CV_ERROR_MSG_WRONG_OPERANDS,
                "Function '"+fname+"' expects binary input as a LIST of 0s and 1s",
                token
            );
            return {};
        }

        auto list = std::static_pointer_cast<CV::DataList>(only);

        if(list->v.size() % 8 != 0){
            cursor->setError(
                CV_ERROR_MSG_WRONG_OPERANDS,
                "Function '"+fname+"' expects binary bit list length to be multiple of 8",
                token
            );
            return {};
        }

        for(int i = 0; i < static_cast<int>(list->v.size()); i += 8){
            unsigned char byte = 0;

            for(int b = 0; b < 8; ++b){
                auto bitData = __cv_file_unwrap(list->v[i + b]);
                if(!bitData || bitData->type != CV::DataType::NUMBER){
                    cursor->setError(
                        CV_ERROR_MSG_WRONG_OPERANDS,
                        "Function '"+fname+"' expects binary bit list to contain NUMBER values 0 or 1",
                        token
                    );
                    return {};
                }

                auto bit = static_cast<int>(std::static_pointer_cast<CV::DataNumber>(bitData)->v);
                if(bit != 0 && bit != 1){
                    cursor->setError(
                        CV_ERROR_MSG_WRONG_OPERANDS,
                        "Function '"+fname+"' expects binary bit list to contain only 0 or 1",
                        token
                    );
                    return {};
                }

                byte = static_cast<unsigned char>((byte << 1) | bit);
            }

            out.push_back(byte);
        }

        return out;
    }

    static std::shared_ptr<CV::Data> __cv_file_bytes_to_bits(
        const std::vector<unsigned char> &bytes,
        const CV::ContextType &ctx
    ){
        auto out = ctx->buildList();

        for(int i = 0; i < static_cast<int>(bytes.size()); ++i){
            unsigned char byte = bytes[i];
            for(int b = 7; b >= 0; --b){
                int bit = (byte >> b) & 1;
                out->v.push_back(std::static_pointer_cast<CV::Data>(ctx->buildNumber(bit)));
            }
        }

        return std::static_pointer_cast<CV::Data>(out);
    }

    static bool __cv_file_seek_abs(
        FILE *fp,
        long offset,
        const std::string &fname,
        const CV::CursorType &cursor,
        const CV::TokenType &token
    ){
        if(offset < 0){
            cursor->setError(
                CV_ERROR_MSG_WRONG_OPERANDS,
                "Function '"+fname+"' expects non-negative offset",
                token
            );
            return false;
        }

        if(std::fseek(fp, offset, SEEK_SET) != 0){
            cursor->setError(
                CV_ERROR_MSG_WRONG_OPERANDS,
                "Function '"+fname+"' failed to seek to requested offset",
                token
            );
            return false;
        }

        return true;
    }

    static long __cv_file_size(
        FILE *fp,
        const std::string &fname,
        const CV::CursorType &cursor,
        const CV::TokenType &token
    ){
        auto current = std::ftell(fp);
        if(current < 0){
            cursor->setError(
                CV_ERROR_MSG_WRONG_OPERANDS,
                "Function '"+fname+"' failed to query current file position",
                token
            );
            return -1;
        }

        if(std::fseek(fp, 0, SEEK_END) != 0){
            cursor->setError(
                CV_ERROR_MSG_WRONG_OPERANDS,
                "Function '"+fname+"' failed to seek to end of file",
                token
            );
            return -1;
        }

        auto size = std::ftell(fp);
        if(size < 0){
            cursor->setError(
                CV_ERROR_MSG_WRONG_OPERANDS,
                "Function '"+fname+"' failed to get file size",
                token
            );
            return -1;
        }

        if(std::fseek(fp, current, SEEK_SET) != 0){
            cursor->setError(
                CV_ERROR_MSG_WRONG_OPERANDS,
                "Function '"+fname+"' failed to restore file position",
                token
            );
            return -1;
        }

        return size;
    }

    static std::shared_ptr<CV::Data> __CV_STD_FILE_OPEN(
        const std::vector<std::pair<std::string, std::shared_ptr<CV::Data>>> &args,
        const CV::ContextType &ctx,
        const CV::CursorType &cursor,
        const CV::TokenType &token
    ){
        const std::string name = "file:open";

        if(!__cv_file_expect_exactly(name, args, 2, cursor, token)){
            return ctx->buildNil();
        }

        auto filePathData = __cv_file_unwrap(args[0].second);
        auto modeData = __cv_file_unwrap(args[1].second);

        if(!__cv_file_expect_type(name, filePathData, CV::DataType::STRING, cursor, token) ||
           !__cv_file_expect_type(name, modeData, CV::DataType::STRING, cursor, token)){
            return ctx->buildNil();
        }

        auto rawPath = std::static_pointer_cast<CV::DataString>(filePathData)->v;
        auto rawMode = std::static_pointer_cast<CV::DataString>(modeData)->v;

        std::string mode;
        if(!__cv_file_parse_mode(rawMode, mode, cursor, token, name)){
            return ctx->buildNil();
        }

        std::filesystem::path absPath;
        try{
            absPath = std::filesystem::absolute(std::filesystem::path(rawPath));
        }catch(...){
            cursor->setError(
                CV_ERROR_MSG_WRONG_OPERANDS,
                "Function '"+name+"' was given an invalid path '"+rawPath+"'",
                token
            );
            return ctx->buildNil();
        }

        FILE *fp = std::fopen(absPath.string().c_str(), "r+b");
        if(fp == nullptr){
            fp = std::fopen(absPath.string().c_str(), "w+b");
        }

        if(fp == nullptr){
            cursor->setError(
                CV_ERROR_MSG_WRONG_OPERANDS,
                "Function '"+name+"' failed to open '"+absPath.string()+"'",
                token
            );
            return ctx->buildNil();
        }

        __cv_file_entry entry;
        entry.fp = fp;
        entry.filename = absPath.filename().string();
        entry.path = absPath.parent_path().string();
        entry.file_path = absPath.string();
        entry.extension = absPath.has_extension() ? absPath.extension().string().substr(1) : "";
        entry.mode = mode;

        int id = GEN_ID();
        {
            std::lock_guard<std::mutex> lock(__cv_file_mutex);
            __cv_file_handles[id] = entry;
        }

        return std::static_pointer_cast<CV::Data>(
            __cv_file_make_descriptor(ctx, id, entry)
        );
    }

    static std::shared_ptr<CV::Data> __CV_STD_FILE_CLOSE(
        const std::vector<std::pair<std::string, std::shared_ptr<CV::Data>>> &args,
        const CV::ContextType &ctx,
        const CV::CursorType &cursor,
        const CV::TokenType &token
    ){
        const std::string name = "file:close";

        if(!__cv_file_expect_exactly(name, args, 1, cursor, token)){
            return ctx->buildNil();
        }

        int fileId = 0;
        __cv_file_entry entry;
        if(!__cv_file_extract_handle(name, args[0].second, fileId, entry, cursor, token)){
            return ctx->buildNil();
        }

        {
            std::lock_guard<std::mutex> lock(__cv_file_mutex);
            if(__cv_file_handles.count(fileId) == 0 || __cv_file_handles[fileId].fp == nullptr){
                cursor->setError(
                    CV_ERROR_MSG_WRONG_OPERANDS,
                    "Function '"+name+"' was given a closed or invalid file descriptor",
                    token
                );
                return ctx->buildNil();
            }

            std::fclose(__cv_file_handles[fileId].fp);
            __cv_file_handles[fileId].fp = nullptr;
        }

        return std::static_pointer_cast<CV::Data>(ctx->buildNumber(1));
    }

    static std::shared_ptr<CV::Data> __CV_STD_FILE_WRITE(
        const std::vector<std::pair<std::string, std::shared_ptr<CV::Data>>> &args,
        const CV::ContextType &ctx,
        const CV::CursorType &cursor,
        const CV::TokenType &token
    ){
        const std::string name = "file:write";

        if(!__cv_file_expect_exactly(name, args, 2, cursor, token)){
            return ctx->buildNil();
        }

        int fileId = 0;
        __cv_file_entry entry;
        if(!__cv_file_extract_handle(name, args[0].second, fileId, entry, cursor, token)){
            return ctx->buildNil();
        }

        if(std::fseek(entry.fp, 0, SEEK_END) != 0){
            cursor->setError(
                CV_ERROR_MSG_WRONG_OPERANDS,
                "Function '"+name+"' failed to seek to end of file",
                token
            );
            return ctx->buildNil();
        }

        if(entry.mode == "BINARY"){
            std::vector<std::pair<std::string, std::shared_ptr<CV::Data>>> oneArg{
                {"", args[1].second}
            };
            auto bytes = __cv_file_bits_to_bytes(oneArg, cursor, token, name);
            if(cursor->error){
                return ctx->buildNil();
            }

            if(!bytes.empty()){
                auto written = std::fwrite(bytes.data(), 1, bytes.size(), entry.fp);
                if(written != bytes.size()){
                    cursor->setError(
                        CV_ERROR_MSG_WRONG_OPERANDS,
                        "Function '"+name+"' failed while writing binary data",
                        token
                    );
                    return ctx->buildNil();
                }
            }
        }else{
            auto input = __cv_file_unwrap(args[1].second);
            if(!__cv_file_expect_type(name, input, CV::DataType::STRING, cursor, token)){
                return ctx->buildNil();
            }

            auto text = std::static_pointer_cast<CV::DataString>(input)->v;
            if(!text.empty()){
                auto written = std::fwrite(text.data(), 1, text.size(), entry.fp);
                if(written != text.size()){
                    cursor->setError(
                        CV_ERROR_MSG_WRONG_OPERANDS,
                        "Function '"+name+"' failed while writing text data",
                        token
                    );
                    return ctx->buildNil();
                }
            }
        }

        std::fflush(entry.fp);
        return std::static_pointer_cast<CV::Data>(ctx->buildNumber(1));
    }

    static std::shared_ptr<CV::Data> __CV_STD_FILE_WRITE_AT(
        const std::vector<std::pair<std::string, std::shared_ptr<CV::Data>>> &args,
        const CV::ContextType &ctx,
        const CV::CursorType &cursor,
        const CV::TokenType &token
    ){
        const std::string name = "file:write-at";

        if(!__cv_file_expect_exactly(name, args, 3, cursor, token)){
            return ctx->buildNil();
        }

        int fileId = 0;
        __cv_file_entry entry;
        if(!__cv_file_extract_handle(name, args[0].second, fileId, entry, cursor, token)){
            return ctx->buildNil();
        }

        auto offsetData = __cv_file_unwrap(args[1].second);
        if(!__cv_file_expect_type(name, offsetData, CV::DataType::NUMBER, cursor, token)){
            return ctx->buildNil();
        }

        long offset = static_cast<long>(std::static_pointer_cast<CV::DataNumber>(offsetData)->v);
        if(!__cv_file_seek_abs(entry.fp, offset, name, cursor, token)){
            return ctx->buildNil();
        }

        if(entry.mode == "BINARY"){
            std::vector<std::pair<std::string, std::shared_ptr<CV::Data>>> oneArg{
                {"", args[2].second}
            };
            auto bytes = __cv_file_bits_to_bytes(oneArg, cursor, token, name);
            if(cursor->error){
                return ctx->buildNil();
            }

            if(!bytes.empty()){
                auto written = std::fwrite(bytes.data(), 1, bytes.size(), entry.fp);
                if(written != bytes.size()){
                    cursor->setError(
                        CV_ERROR_MSG_WRONG_OPERANDS,
                        "Function '"+name+"' failed while writing binary data",
                        token
                    );
                    return ctx->buildNil();
                }
            }
        }else{
            auto input = __cv_file_unwrap(args[2].second);
            if(!__cv_file_expect_type(name, input, CV::DataType::STRING, cursor, token)){
                return ctx->buildNil();
            }

            auto text = std::static_pointer_cast<CV::DataString>(input)->v;
            if(!text.empty()){
                auto written = std::fwrite(text.data(), 1, text.size(), entry.fp);
                if(written != text.size()){
                    cursor->setError(
                        CV_ERROR_MSG_WRONG_OPERANDS,
                        "Function '"+name+"' failed while writing text data",
                        token
                    );
                    return ctx->buildNil();
                }
            }
        }

        std::fflush(entry.fp);
        return std::static_pointer_cast<CV::Data>(ctx->buildNumber(1));
    }

    static std::shared_ptr<CV::Data> __CV_STD_FILE_READ(
        const std::vector<std::pair<std::string, std::shared_ptr<CV::Data>>> &args,
        const CV::ContextType &ctx,
        const CV::CursorType &cursor,
        const CV::TokenType &token
    ){
        const std::string name = "file:read";

        if(!__cv_file_expect_exactly(name, args, 1, cursor, token)){
            return ctx->buildNil();
        }

        int fileId = 0;
        __cv_file_entry entry;
        if(!__cv_file_extract_handle(name, args[0].second, fileId, entry, cursor, token)){
            return ctx->buildNil();
        }

        long size = __cv_file_size(entry.fp, name, cursor, token);
        if(cursor->error){
            return ctx->buildNil();
        }

        if(!__cv_file_seek_abs(entry.fp, 0, name, cursor, token)){
            return ctx->buildNil();
        }

        std::vector<unsigned char> bytes(static_cast<std::size_t>(size));
        if(size > 0){
            auto read = std::fread(bytes.data(), 1, bytes.size(), entry.fp);
            if(read != bytes.size()){
                cursor->setError(
                    CV_ERROR_MSG_WRONG_OPERANDS,
                    "Function '"+name+"' failed while reading file",
                    token
                );
                return ctx->buildNil();
            }
        }

        if(entry.mode == "BINARY"){
            return __cv_file_bytes_to_bits(bytes, ctx);
        }

        return std::static_pointer_cast<CV::Data>(
            ctx->buildString(std::string(bytes.begin(), bytes.end()))
        );
    }

    static std::shared_ptr<CV::Data> __CV_STD_FILE_READ_AT(
        const std::vector<std::pair<std::string, std::shared_ptr<CV::Data>>> &args,
        const CV::ContextType &ctx,
        const CV::CursorType &cursor,
        const CV::TokenType &token
    ){
        const std::string name = "file:read-at";

        if(!__cv_file_expect_exactly(name, args, 3, cursor, token)){
            return ctx->buildNil();
        }

        int fileId = 0;
        __cv_file_entry entry;
        if(!__cv_file_extract_handle(name, args[0].second, fileId, entry, cursor, token)){
            return ctx->buildNil();
        }

        auto offsetData = __cv_file_unwrap(args[1].second);
        auto amountData = __cv_file_unwrap(args[2].second);

        if(!__cv_file_expect_type(name, offsetData, CV::DataType::NUMBER, cursor, token) ||
           !__cv_file_expect_type(name, amountData, CV::DataType::NUMBER, cursor, token)){
            return ctx->buildNil();
        }

        long offset = static_cast<long>(std::static_pointer_cast<CV::DataNumber>(offsetData)->v);
        long amount = static_cast<long>(std::static_pointer_cast<CV::DataNumber>(amountData)->v);

        if(amount < 0){
            cursor->setError(
                CV_ERROR_MSG_WRONG_OPERANDS,
                "Function '"+name+"' expects non-negative amount",
                token
            );
            return ctx->buildNil();
        }

        if(!__cv_file_seek_abs(entry.fp, offset, name, cursor, token)){
            return ctx->buildNil();
        }

        std::vector<unsigned char> bytes(static_cast<std::size_t>(amount));
        if(amount > 0){
            auto read = std::fread(bytes.data(), 1, bytes.size(), entry.fp);
            bytes.resize(read);
        }

        if(entry.mode == "BINARY"){
            return __cv_file_bytes_to_bits(bytes, ctx);
        }

        return std::static_pointer_cast<CV::Data>(
            ctx->buildString(std::string(bytes.begin(), bytes.end()))
        );
    }
}

static std::shared_ptr<CV::Data> __CV_STD_FILE_EXISTS(
    const std::vector<std::pair<std::string, std::shared_ptr<CV::Data>>> &args,
    const CV::ContextType &ctx,
    const CV::CursorType &cursor,
    const CV::TokenType &token
){
    const std::string name = "file:exists";

    std::filesystem::path path;
    if(!__cv_file_extract_path_string(name, args, path, cursor, token)){
        return ctx->buildNil();
    }

    std::error_code ec;
    bool exists = std::filesystem::exists(path, ec);
    if(ec){
        cursor->setError(
            CV_ERROR_MSG_WRONG_OPERANDS,
            "Function '"+name+"' failed to query path '"+path.string()+"'",
            token
        );
        return ctx->buildNil();
    }

    return std::static_pointer_cast<CV::Data>(
        ctx->buildNumber(exists ? 1 : 0)
    );
}

static std::shared_ptr<CV::Data> __CV_STD_FILE_GET_SIZE(
    const std::vector<std::pair<std::string, std::shared_ptr<CV::Data>>> &args,
    const CV::ContextType &ctx,
    const CV::CursorType &cursor,
    const CV::TokenType &token
){
    const std::string name = "file:get-size";

    std::filesystem::path path;
    if(!__cv_file_extract_path_string(name, args, path, cursor, token)){
        return ctx->buildNil();
    }

    std::error_code ec;
    if(!std::filesystem::exists(path, ec) || ec){
        cursor->setError(
            CV_ERROR_MSG_WRONG_OPERANDS,
            "Function '"+name+"' path '"+path.string()+"' does not exist",
            token
        );
        return ctx->buildNil();
    }

    auto size = std::filesystem::file_size(path, ec);
    if(ec){
        cursor->setError(
            CV_ERROR_MSG_WRONG_OPERANDS,
            "Function '"+name+"' failed to get file size for '"+path.string()+"'",
            token
        );
        return ctx->buildNil();
    }

    return std::static_pointer_cast<CV::Data>(
        ctx->buildNumber(static_cast<CV_NUMBER>(size))
    );
}

static std::shared_ptr<CV::Data> __CV_STD_FILE_GET_LAST_MODIFIED(
    const std::vector<std::pair<std::string, std::shared_ptr<CV::Data>>> &args,
    const CV::ContextType &ctx,
    const CV::CursorType &cursor,
    const CV::TokenType &token
){
    const std::string name = "file:get-last-modified";

    std::filesystem::path path;
    if(!__cv_file_extract_path_string(name, args, path, cursor, token)){
        return ctx->buildNil();
    }

    std::error_code ec;
    if(!std::filesystem::exists(path, ec) || ec){
        cursor->setError(
            CV_ERROR_MSG_WRONG_OPERANDS,
            "Function '"+name+"' path '"+path.string()+"' does not exist",
            token
        );
        return ctx->buildNil();
    }

    auto ft = std::filesystem::last_write_time(path, ec);
    if(ec){
        cursor->setError(
            CV_ERROR_MSG_WRONG_OPERANDS,
            "Function '"+name+"' failed to get last modified time for '"+path.string()+"'",
            token
        );
        return ctx->buildNil();
    }

    return std::static_pointer_cast<CV::Data>(
        ctx->buildNumber(__cv_file_filetime_to_epoch_seconds(ft))
    );
}

static std::shared_ptr<CV::Data> __CV_STD_FILE_GET_CREATED_AT(
    const std::vector<std::pair<std::string, std::shared_ptr<CV::Data>>> &args,
    const CV::ContextType &ctx,
    const CV::CursorType &cursor,
    const CV::TokenType &token
){
    const std::string name = "file:get-created-at";

    std::filesystem::path path;
    if(!__cv_file_extract_path_string(name, args, path, cursor, token)){
        return ctx->buildNil();
    }

    std::error_code ec;
    if(!std::filesystem::exists(path, ec) || ec){
        cursor->setError(
            CV_ERROR_MSG_WRONG_OPERANDS,
            "Function '"+name+"' path '"+path.string()+"' does not exist",
            token
        );
        return ctx->buildNil();
    }

    CV_NUMBER epoch = 0;
    if(!__cv_file_get_created_at_seconds(path, epoch, cursor, token, name)){
        return ctx->buildNil();
    }

    return std::static_pointer_cast<CV::Data>(
        ctx->buildNumber(epoch)
    );
}

static std::shared_ptr<CV::Data> __CV_STD_FILE_DELETE(
    const std::vector<std::pair<std::string, std::shared_ptr<CV::Data>>> &args,
    const CV::ContextType &ctx,
    const CV::CursorType &cursor,
    const CV::TokenType &token
){
    const std::string name = "file:delete";

    std::filesystem::path path;
    if(!__cv_file_extract_path_string(name, args, path, cursor, token)){
        return ctx->buildNil();
    }

    std::error_code ec;
    bool removed = std::filesystem::remove(path, ec);
    if(ec){
        cursor->setError(
            CV_ERROR_MSG_WRONG_OPERANDS,
            "Function '"+name+"' failed to delete '"+path.string()+"'",
            token
        );
        return ctx->buildNil();
    }

    return std::static_pointer_cast<CV::Data>(
        ctx->buildNumber(removed ? 1 : 0)
    );
}

static std::shared_ptr<CV::Data> __CV_STD_FILE_GET_FILENAME(
    const std::vector<std::pair<std::string, std::shared_ptr<CV::Data>>> &args,
    const CV::ContextType &ctx,
    const CV::CursorType &cursor,
    const CV::TokenType &token
){
    const std::string name = "file:get-filename";

    std::filesystem::path path;
    if(!__cv_file_extract_path_string(name, args, path, cursor, token)){
        return ctx->buildNil();
    }

    return std::static_pointer_cast<CV::Data>(
        ctx->buildString(path.filename().string())
    );
}

static std::shared_ptr<CV::Data> __CV_STD_FILE_GET_EXTENSION(
    const std::vector<std::pair<std::string, std::shared_ptr<CV::Data>>> &args,
    const CV::ContextType &ctx,
    const CV::CursorType &cursor,
    const CV::TokenType &token
){
    const std::string name = "file:get-extension";

    std::filesystem::path path;
    if(!__cv_file_extract_path_string(name, args, path, cursor, token)){
        return ctx->buildNil();
    }

    std::string ext = path.has_extension() ? path.extension().string() : "";
    if(!ext.empty() && ext[0] == '.'){
        ext.erase(0, 1);
    }

    return std::static_pointer_cast<CV::Data>(
        ctx->buildString(ext)
    );
}

extern "C" void _CV_REGISTER_LIBRARY(
    const CV::ContextType &ctx,
    const CV::CursorType &cursor
){
    (void)cursor;

    ctx->registerFunction("file:open", {"file_path", "mode"}, __CV_STD_FILE_OPEN);
    ctx->registerFunction("file:close", {"subject"}, __CV_STD_FILE_CLOSE);
    ctx->registerFunction("file:write", {"subject", "input"}, __CV_STD_FILE_WRITE);
    ctx->registerFunction("file:write-at", {"subject", "offset", "input"}, __CV_STD_FILE_WRITE_AT);
    ctx->registerFunction("file:read", {"subject"}, __CV_STD_FILE_READ);
    ctx->registerFunction("file:read-at", {"subject", "offset", "amount"}, __CV_STD_FILE_READ_AT);
    ctx->registerFunction("file:exists", {"file_path"}, __CV_STD_FILE_EXISTS);
    ctx->registerFunction("file:get-size", {"file_path"}, __CV_STD_FILE_GET_SIZE);
    ctx->registerFunction("file:get-last-modified", {"file_path"}, __CV_STD_FILE_GET_LAST_MODIFIED);
    ctx->registerFunction("file:get-created-at", {"file_path"}, __CV_STD_FILE_GET_CREATED_AT);
    ctx->registerFunction("file:delete", {"file_path"}, __CV_STD_FILE_DELETE);
    ctx->registerFunction("file:get-filename", {"file_path"}, __CV_STD_FILE_GET_FILENAME);
    ctx->registerFunction("file:get-extension", {"file_path"}, __CV_STD_FILE_GET_EXTENSION);    
}