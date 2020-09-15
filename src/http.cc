#include <unistd.h>
#include <sys/types.h>
#include <netdb.h>
#include <arpa/inet.h>

#include <cstring>
#include <climits>
#include <stdexcept>
#include <regex>
#include <iostream>
#include <fstream>

#include "http.h"

#define VALID_HTTP_URL_REGEX    "^(?:([A-Za-z]+)(?::\\/\\/))?(?:([A-Za-z0-9\\.\\-_]+)(?::([0-9]{1,5}))?)\\/((?:[A-Za-z0-9\\.\\-_%]*\\/)*([A-Za-z0-9\\.\\-_%]+)(?:\\?[A-Za-z0-9\\.\\-_=&,#%]*)?)$"
#define DOWNLOAD_RCV_TIMEOUT_S  5
#define MAX_FILE_NAME_TRYOUTS   UINT_MAX
#define RCV_SMALL_BUFF_SIZE     64
#define RCV_LARGE_BUFF_SIZE     1024
#define RCV_CHUNK_BUFF_SIZE     4096

namespace http
{
    static void str_tolower(std::string& str)
    {
        std::transform(str.begin(), str.end(), str.begin(),
            [](unsigned char c){ return std::tolower(c); });
    }

    Downloader::Downloader(ipgrogress_ptr_t pr) noexcept :
        progress(std::move(pr))
    {

    }

    void Downloader::dowload(const std::string& url,
                             const std::filesystem::path& download_dir,
                             const std::filesystem::path& file_name,
                             bool rewrite)
    {
        auto info = create_request_info(url);

        if (info.protocol != "http")
        {
            std::string msg = "Unsupported protocol: ";
            msg += info.protocol;
            throw std::runtime_error(msg);
        }

        auto request = create_get_request(info);

        Connection connection(progress);
        connection.connect(info.host, info.port);
        connection.send_request(request);
        auto status = connection.retrieve_http_status_line();

        if (status.status_code == 200)
        {
            auto outdir = download_dir.empty() ? "." : download_dir;
            auto outname = file_name.empty() ? std::filesystem::path(info.file_name) : file_name.filename();
            auto path = rewrite ? outdir / outname : get_unique_file_path(outdir, outname);
            std::ofstream of(path, std::ios::binary | std::ios::trunc);

            if (of.is_open())
            {
                connection.download(of);
            }
            else
            {
                std::string msg = "Unable to open file '";
                msg += path.string();
                msg += "'.";
                throw std::runtime_error(msg);
            }

            return;
        }

        std::string msg = "Unsuccessful request. Status code: ";
        msg += std::to_string(status.status_code);
        msg += ' ';
        msg += status.status_text;
        msg += '.';

        if (status.status_code == 301 ||
            status.status_code == 302 ||
            status.status_code == 303 ||
            status.status_code == 305 ||
            status.status_code == 307 ||
            status.status_code == 308 )
         {
             auto headers = connection.retrieve_headers();
             auto it = headers.find("location");

             if (it != headers.end())
             {
                 msg += " New location: ";
                 msg += it->second;
             }
         }

        throw std::runtime_error(msg);
    }

    Downloader::Request_Info Downloader::create_request_info(const std::string& url)
    {
        if (url.empty())
        {
            throw std::invalid_argument("URL is not specified.");
        }

        std::regex valid_http_url_regex(VALID_HTTP_URL_REGEX);
        std::smatch match;

        if (!std::regex_match(url, match, valid_http_url_regex))
        {
            throw std::invalid_argument("Invalid URL.");
        }

        Request_Info info;
        info.protocol = match[1].str();
        info.host = match[2].str();

        auto port_str = match[3].str();

        if (port_str.empty())
        {
            info.port = 80;
        }
        else
        {
            auto port_val = std::strtoul(port_str.c_str(), nullptr, 10);

            if (0 == port_val ||
                ULONG_MAX == port_val ||
                USHRT_MAX < port_val)
            {
                std::string msg = "Invalid port value: ";
                msg += port_str;
                throw std::invalid_argument(msg);
            }

            info.port = static_cast<std::uint16_t>(port_val);
        }

        info.url = match[4].str();
        info.file_name = match[5].str();

        return info;
    }

    std::string Downloader::create_get_request(const Downloader::Request_Info& info)
    {
        std::string request = "GET /";
        request += info.url;
        request += " HTTP/1.1\r\nHost: ";
        request += info.host;
        request += "\r\nUser-Agent: downloader\r\nAccept: */*\r\nConnection: keep-alive\r\n\r\n";
        return request;
    }

    std::filesystem::path Downloader::get_unique_file_path(const std::filesystem::path& dir,
                                                           const std::filesystem::path& file_name)
    {
        if (!std::filesystem::exists(dir))
        {
            std::filesystem::create_directories(dir);
        }

        std::filesystem::path file_path = dir / file_name.filename();
        auto stem = file_path.stem().string();
        auto extension = file_path.extension().string();

        unsigned i = 1;
        while (i < MAX_FILE_NAME_TRYOUTS && std::filesystem::exists(file_path))
        {
            auto distinct_file_name = stem;
            distinct_file_name += " (";
            distinct_file_name += std::to_string(i);
            distinct_file_name += ")";
            distinct_file_name += extension;
            file_path.replace_filename(distinct_file_name);
            ++i;
        }

        if (MAX_FILE_NAME_TRYOUTS == i)
        {
            std::string msg = "Unable to obtain unique name for file '";
            msg += file_name;
            msg += "'. Try to change download directory.";
            throw std::runtime_error(msg);
        }

        return file_path;
    }

    Downloader::Connection::Connection(ipgrogress_ptr_t& pr) noexcept :
        progress(pr)
    {

    }

    Downloader::Connection::~Connection()
    {
        close();
    }

    void Downloader::Connection::connect(const std::string& host, uint16_t port)
    {
        sockaddr_in sin;
        sin.sin_family = AF_INET;
        sin.sin_port = htons(port);
        sin.sin_addr = resolve_name(host);

        sock = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

        if (sock < 0)
        {
            std::string msg = "Unable to create socket: ";
            msg += ::strerror(errno);
            throw std::runtime_error(msg);
        }

        timeval tv;
        tv.tv_sec = DOWNLOAD_RCV_TIMEOUT_S;
        tv.tv_usec = 0;

        if (::setsockopt (sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv, sizeof(tv)) == -1)
        {
            close();
            std::string msg = "Could not set socket option SO_RCVTIMEO: ";
            msg += ::strerror(errno);
            throw std::runtime_error(msg);
        }

        if (::connect(sock, (const sockaddr*) &sin, sizeof(sin)) < 0)
        {
            close();
            std::string msg = "Unable to connect to ";
            msg += host;
            msg += " (";
            msg += ::inet_ntoa (sin.sin_addr);
            msg += ") : ";
            msg += ::strerror(errno);
            throw std::runtime_error(msg);
        }
    }

    void Downloader::Connection::send_request(const std::string& request) const
    {
        auto bytes_sent = ::send(sock, request.c_str(), request.length(), MSG_NOSIGNAL);

        if ((unsigned int) bytes_sent < request.length())
        {
            std::string msg = "Unable to send request: ";
            msg += strerror(errno);
            throw std::runtime_error(msg);
        }
    }

    Downloader::Connection::Status_Line Downloader::Connection::retrieve_http_status_line()
    {
        char buff[RCV_SMALL_BUFF_SIZE];
        bool cr_found = false;
        bool lf_found = false;
        std::string status_line;
        ssize_t bytes_read;
        ssize_t i = 0;

        while (!(cr_found && lf_found))
        {
            check_if_canceled();

            bytes_read = ::recv(sock, buff, sizeof(buff), 0);

            if (bytes_read < 0)
            {
                if (errno == EINTR)
                    continue;

                std::string msg = "Unable to check status code: ";
                msg += strerror(errno);
                throw std::runtime_error(msg);
            }

            if (bytes_read == 0)
            {
                throw std::runtime_error("Invalid server response: Unable to retrieve status line");
            }

            i = 0;

            if (cr_found)
            {
                if (buff[i] == '\n')
                {
                    lf_found = true;
                    ++i;
                }
                else
                {
                    throw std::domain_error("Invalid server response");
                }
            }
            else
            {
                while (i < bytes_read)
                {
                    if (buff[i] == '\r')
                    {
                        cr_found = true;

                        if (++i < RCV_SMALL_BUFF_SIZE)
                        {
                            if (buff[i] == '\n')
                            {
                                lf_found = true;
                                ++i;
                                break;
                            }
                            else
                            {
                                throw std::domain_error("Invalid server response");
                            }
                        }
                    }

                    ++i;
                }
            }

            size_t end = i;

            if (cr_found)
                --end;

            if (lf_found)
                --end;

            status_line.append(buff, end);
        }

        buffer.append(&buff[i], bytes_read - i);

        size_t len = status_line.length();
        size_t s = 0;
        size_t e;

        Status_Line status;

        while (s < len && std::isspace(status_line[s])) ++s;

        e = s;
        while (e < len && !std::isspace(status_line[e])) ++e;

        status.protocol_version.append(status_line, s, e - s);

        s = e;
        while (s < len && std::isspace(status_line[s])) ++s;

        e = s;
        while (e < len && !std::isspace(status_line[e])) ++e;

        std::string code;
        code.append(status_line, s, e - s);
        status.status_code = std::strtoul(code.c_str(), nullptr, 10);

        s = e;
        while (s < len && std::isspace(status_line[s])) ++s;

        status.status_text.append(status_line, s, len - s);

        return status;
    }

    Downloader::Connection::header_list_t Downloader::Connection::retrieve_headers()
    {
        std::string headers;

        const char* marker = "\r\n\r\n";
        const auto marker_len = std::strlen(marker);

        std::string::size_type start_pos = 0;
        std::string::size_type marker_pos = std::string::npos;

        while (true)
        {
            check_if_canceled();

            marker_pos = buffer.find(marker, start_pos);

            if (marker_pos != std::string::npos)
            {
                headers = buffer.substr(0, marker_pos + marker_len);
                auto body = buffer.substr(marker_pos + marker_len);
                buffer.swap(body);
                break;
            }

            start_pos = buffer.length() > marker_len ? buffer.length() - marker_len + 1 : 0;

            char buff[RCV_LARGE_BUFF_SIZE];

            auto bytes_read = ::recv(sock, buff, sizeof(buff), 0);

            if (bytes_read < 0)
            {
                if (errno == EINTR)
                    continue;

                std::string msg = "Unable to retrieve http headers: ";
                msg += strerror(errno);
                throw std::runtime_error(msg);
            }

            if (bytes_read == 0)
            {
                throw std::runtime_error("Invalid server response: Unable to retrieve headers");
            }

            buffer.append(buff, bytes_read);
        }

        return parse_headers(headers);
    }

    void Downloader::Connection::download(std::ofstream& of)
    {
        auto headers = retrieve_headers();

        auto it = headers.find("transfer-encoding");

        if (it == headers.end())
        {
            it = headers.find("content-length");

            if (it == headers.end())
            {
                throw std::runtime_error("Invalid headers. Neither Transfer-Encoding nor Content-Length are present.");
            }

            auto length = std::strtoll(it->second.c_str(), nullptr, 10);

            if (progress)
            {
                progress->start();
                progress->set_total(length);
            }

            download_content(of, length);

            if (progress)
            {
                progress->stop();
            }
        }
        else
        {
            auto encoding = it->second;
            str_tolower(encoding);

            if (encoding.find("chunked") == std::string::npos)
            {
                std::string msg = "Unsupported Transfer-Encoding: ";
                msg += encoding;
                throw std::runtime_error(msg);
            }

            if (progress)
            {
                progress->start();
                progress->set_total(0);
            }

            download_chunks(of);

            if (progress)
            {
                progress->stop();
            }
        }
    }

    in_addr Downloader::Connection::resolve_name(const std::string& hostname)
    {
        addrinfo hint {0, AF_INET, SOCK_STREAM, 0, 0, nullptr, nullptr, nullptr};
        addrinfo* info = nullptr;

        auto result = ::getaddrinfo (hostname.c_str (), nullptr, &hint, &info);

        if (result)
        {
            std::string msg = "Can't resolve host name.";
            throw std::runtime_error(msg);
        }

        in_addr addr = reinterpret_cast<sockaddr_in*>(info->ai_addr)->sin_addr;

        ::freeaddrinfo(info);

        return addr;
    }

    Downloader::Connection::header_list_t Downloader::Connection::parse_headers(const std::string& headers)
    {
        header_list_t list;

        size_t len = headers.length();
        size_t i = 0;

        bool key_part = true;
        bool end_line = false;
        bool space = false;
        bool inside = false;
        std::string key;
        std::string val;

        while (i < len)
        {
            auto ch = headers[i];

            switch (ch)
            {
                case ':':
                {
                    if (key_part)
                    {
                        key_part = false;

                        /* skip spaces */
                        inside = false;
                        space = false;
                    }
                    else
                    {
                        val.push_back(ch);
                    }

                    break;
                }

                case '\r':
                {
                    end_line = true;
                    break;
                }

                case '\n':
                {
                    if (!end_line)
                    {
                        std::string msg = "Invalid server response: Unable to parse headers.";
                        throw std::runtime_error(msg);
                    }

                    end_line = false;
                    key_part = true;

                    if (!key.empty()) {
                        str_tolower(key);
                        list.insert(std::pair{ std::move(key), std::move(val) });
                    }

                    /* skip spaces */
                    inside = false;
                    space = false;
                    break;
                }

                case '\t':
                case ' ':
                {
                    /* normalize spacing */
                    if (inside)
                        space = true;

                    break;
                }

                default:
                {
                    inside = true;

                    if (key_part)
                    {
                        if (space)
                        {
                            key.push_back(' ');
                            space = false;
                        }

                        key.push_back(ch);
                    }
                    else
                    {
                        if (space)
                        {
                            val.push_back(' ');
                            space = false;
                        }

                        val.push_back(ch);
                    }
                }
            }

            ++i;
        }

        return list;
    }

    void Downloader::Connection::write(std::ofstream& of, const char* buff, size_t len) noexcept
    {
        of.write(buff, len);

        if (progress)
        {
            progress->add_progress(len);
        }
    }

    void Downloader::Connection::close() noexcept
    {
        if (sock > 0)
        {
            ::close(sock);
            sock = -1;
        }

        if (progress)
        {
            progress->stop();
        }
    }

    void Downloader::Connection::check_if_canceled()
    {
        if (progress && progress->is_canceled())
            throw std::runtime_error("Canceled.");
    }

    void Downloader::Connection::download_content(std::ofstream& of, ssize_t len)
    {
        if (len < static_cast<ssize_t>(buffer.length()))
        {
            throw std::runtime_error("Unable to get content");
        }

        write(of, buffer.data(), buffer.length());
        len -= buffer.length();
        buffer.clear();

        while (len)
        {
            check_if_canceled();

            char buff[RCV_CHUNK_BUFF_SIZE];

            auto bytes_read = ::recv(sock, buff, sizeof(buff), 0);

            if (bytes_read < 0)
            {
                if (errno == EINTR)
                    continue;

                std::string msg = "Unable to download content: ";
                msg += strerror(errno);
                throw std::runtime_error(msg);
            }

            if (bytes_read == 0)
            {
                throw std::runtime_error("Invalid server response: Unable to download content.");
            }

            write(of, buff, bytes_read);
            len = len > bytes_read ? len - bytes_read : 0;
        }
    }

    void Downloader::Connection::download_chunks(std::ofstream& of)
    {
        while (size_t len = get_chunk_length())
        {
            download_chunk(of, len);
        }
    }

    ssize_t Downloader::Connection::get_chunk_length()
    {
        ssize_t length = 0;

        const char* marker = "\r\n";
        const auto marker_len = std::strlen(marker);

        std::string::size_type find_start_pos = 0;
        std::string::size_type number_start_pos = 0;
        std::string::size_type marker_pos = std::string::npos;

        while (true)
        {
            check_if_canceled();

            marker_pos = buffer.find(marker, find_start_pos);

            if (marker_pos != std::string::npos)
            {
                /* skip leading crlf */
                if (marker_pos == 0)
                {
                    number_start_pos = marker_len;
                    find_start_pos = marker_len;
                    continue;
                }
                else
                {
                    length = std::strtoll(buffer.substr(number_start_pos, marker_pos).c_str(), nullptr, 16);
                    auto body = buffer.substr(marker_pos + marker_len);
                    buffer.swap(body);
                    break;
                }
            }

            find_start_pos = buffer.length() > marker_len ? buffer.length() - marker_len + 1 : 0;

            char buff[RCV_CHUNK_BUFF_SIZE];

            auto bytes_read = ::recv(sock, buff, sizeof(buff), 0);

            if (bytes_read < 0)
            {
                if (errno == EINTR)
                    continue;

                std::string msg = "Unable to obtain chunk length: ";
                msg += strerror(errno);
                throw std::runtime_error(msg);
            }

            if (bytes_read == 0)
            {
                throw std::runtime_error("Invalid server response: Unable to obtain chunk length.");
            }

            buffer.append(buff, bytes_read);
        }

        return length;
    }

    void Downloader::Connection::download_chunk(std::ofstream& of, ssize_t len)
    {
        if (len < static_cast<ssize_t>(buffer.length()))
        {
            write(of, buffer.data(), len);
            auto tail = buffer.substr(len);
            buffer.swap(tail);
            return;
        }

        write(of, buffer.data(), buffer.length());
        len -= buffer.length();
        buffer.clear();

        while (len)
        {
            check_if_canceled();

            char buff[RCV_CHUNK_BUFF_SIZE];

            auto bytes_read = ::recv(sock, buff, sizeof(buff), 0);

            if (bytes_read < 0)
            {
                if (errno == EINTR)
                    continue;

                std::string msg = "Unable to download chunk: ";
                msg += strerror(errno);
                throw std::runtime_error(msg);
            }

            if (bytes_read == 0)
            {
                throw std::runtime_error("Invalid server response: Unable to download chunk.");
            }

            if (len < bytes_read)
            {
                write(of, buff, len);
                buffer.append(&buff[len], bytes_read - len);
                break;
            }

            write(of, buff, bytes_read);
            len = len - bytes_read;
        }
    }
}
