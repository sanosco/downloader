#ifndef PROGRESS_H
#define PROGRESS_H

#include "iprogress.h"

namespace http
{
    class Progress : public IProgress
    {
    public:
        ~Progress();

        void start() noexcept override;
        void stop() noexcept override;
        void set_total(size_t t) noexcept override;
        void add_progress(size_t c) noexcept override;
        bool is_canceled() noexcept override;

        static void cancel() noexcept;

    private:
        size_t total = 0;
        size_t current = 0;
        bool started = false;

        static bool canceled;
    };
}



#endif // PROGRESS_H
