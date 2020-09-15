#ifndef IPROGRESS_H
#define IPROGRESS_H

#include <cstring>
#include <memory>

namespace http
{
    struct IProgress
    {
        virtual ~IProgress() = default;

        virtual void start() = 0;
        virtual void stop() = 0;
        virtual void set_total(size_t ) = 0;
        virtual void add_progress(size_t) = 0;
        virtual bool is_canceled() = 0;
    };

    using ipgrogress_ptr_t = std::unique_ptr<IProgress>;
}

#endif // IPROGRESS_H
