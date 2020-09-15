#ifndef HTTP_H
#define HTTP_H

#include <netinet/in.h>

#include <filesystem>
#include <vector>
#include <unordered_map>

#include "iprogress.h"

/*
 * RFC 2616 - "Hypertext Transfer Protocol -- HTTP/1.1"
 * http://www.w3.org/Protocols/rfc2616/rfc2616.html
 *
 * RFC 7230 - "Hypertext Transfer Protocol (HTTP/1.1): Message Syntax and Routing"
 * https://www.ietf.org/rfc/rfc7230.html
*/

namespace http
{
    class Downloader
    {
    public:
        Downloader(ipgrogress_ptr_t pr) noexcept;

        void dowload(const std::string& url,
                     const std::filesystem::path& download_dir,
                     const std::filesystem::path& file_name,
                     bool rewrite);

    private:
        struct Request_Info
        {
            std::string protocol;
            std::string host;
            std::string url;
            std::string file_name;
            std::uint16_t port;
        };

        static Request_Info create_request_info(const std::string& url);
        static std::string create_get_request(const Request_Info& info);

        class Connection
        {
        public:
            struct Status_Line
            {
                std::string protocol_version;
                unsigned status_code;
                std::string status_text;
            };

            using header_list_t = std::unordered_multimap<std::string, std::string>;

        public:
            Connection(ipgrogress_ptr_t& pr) noexcept;
            ~Connection();

            void connect(const std::string& host, std::uint16_t port);
            void send_request(const std::string& request) const;
            Status_Line retrieve_http_status_line();
            header_list_t retrieve_headers();
            void download(std::ofstream& of);

        private:
            static in_addr resolve_name(const std::string& hostname);
            static header_list_t parse_headers(const std::string& headers);

            void write(std::ofstream& of, const char* buff, size_t len) noexcept;
            void close() noexcept;
            void check_if_canceled();

            void download_content(std::ofstream& of, ssize_t len);
            void download_chunks(std::ofstream& of);
            ssize_t get_chunk_length();
            void download_chunk(std::ofstream& of, ssize_t len);

        private:
            int sock = -1;
            std::string buffer;
            ipgrogress_ptr_t& progress;
        };

    private:
        std::filesystem::path get_unique_file_path(const std::filesystem::path& dir,
                                                   const std::filesystem::path& file_name);

    private:
        ipgrogress_ptr_t progress;
    };
}

#endif // HTTP_H
