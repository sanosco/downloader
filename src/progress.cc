#include <iostream>
#include <iomanip>

#include "progress.h"

#define clearln "\r"
#define hidecur "\e[?25l"
#define showcur "\e[?25h"

namespace http
{
    bool Progress::canceled = false;

    Progress::~Progress()
    {
        stop();
    }

    void Progress::start() noexcept
    {
        std::cout << hidecur;
        started = true;
    }

    void Progress::stop() noexcept
    {
        if (started)
        {
            std::cout << std::endl << showcur;
            started = false;
        }

    }

    void Progress::set_total(size_t t) noexcept
    {
        total = t;
    }

    void Progress::add_progress(size_t c) noexcept
    {
        current += c;
        std::string total_str = total ? std::to_string(total) : "-";
        std::string percent = total ? std::to_string( current * 100 / total) : "- ";
        percent += '%';
        std::cout << clearln
                  << std::setw(10)<< current
                  << " / " << std::setw(10) << total_str
                  << std::setw(10) << percent;
    }

    bool Progress::is_canceled() noexcept
    {
        return canceled;
    }

    void Progress::cancel() noexcept
    {
        canceled = true;
    }
}
