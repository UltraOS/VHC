#pragma once

#include <iostream>

class Logger {
public:
    enum class Level {
        INFO,
        WARN,
        ERROR
    };

    friend bool operator<(Level l, Level r) { return static_cast<int>(l) < static_cast<int>(r); }

    static Logger& the()
    {
        static Logger s_logger;

        return s_logger;
    }

    void set_level(Level l) { m_level = l; }

    template <typename T>
    Logger& log(const T& value)
    {
        std::cout << value;

        return *this;
    }

    template <typename... Args>
    Logger& log(Level l, const Args&... args)
    {
        if (l < m_level)
            return *this;

        switch (l) {
        case Level::INFO:
            log("INFO: ");
            break;
        case Level::WARN:
            log("WARNING: ");
            break;
        case Level::ERROR:
            log("ERROR: ");
            break;
        }

        (log(args), ...);

        log("\n");

        return *this;
    }

    template <typename... Args>
    Logger& info(const Args&... args)
    {
        log(Level::INFO, args...);
        return *this;
    }

    template <typename... Args>
    Logger& warning(const Args&... args)
    {
        log(Level::WARN, args...);
        return *this;
    }

    template <typename... Args>
    Logger& error(const Args&... args)
    {
        log(Level::ERROR, args...);
        return *this;
    }

private:
    Level m_level { Level::WARN };
};
