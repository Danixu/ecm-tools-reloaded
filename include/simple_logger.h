/*******************************************************************************
 * 
 * Created by Daniel Carrasco at https://www.electrosoftcloud.com
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 ******************************************************************************/

//#include <cstdint>
#include <cstdio>
#include <cstdarg>
//#include <ctime>

#ifndef _H_SIMPLELOGGER_LOADED
#define _H_SIMPLELOADER_LOADED

namespace simplelogger {
    enum loglevel {
        SIMPLELOGGER_LEVEL_SILENT = 0,
        SIMPLELOGGER_LEVEL_INFO,
        SIMPLELOGGER_LEVEL_WARNING,
        SIMPLELOGGER_LEVEL_ERROR,
        SIMPLELOGGER_LEVEL_DEBUG,
    };

    class logger {
        private:
            uint8_t _log_level;

        public:
            logger(loglevel log_level = (loglevel)0);
            void set_log_level(loglevel log_level);
            // With and without format
            void log(loglevel log_level, const char * format, ...);
            void log(loglevel log_level, const char * format, va_list args);
            // Wrappers
            //void info(const char * format);
            void info(const char * format, ...);
            void warning(const char * format, ...);
            void error(const char * format, ...);
            void debug(const char * format, ...);

    };
}

#endif