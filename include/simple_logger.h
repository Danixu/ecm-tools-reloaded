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

#include <stdint.h>
#include <string>
#include <time.h>

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
            logger(uint8_t log_level = 0);
            void set_log_level(uint8_t log_level);
            void log(std::string message, loglevel log_level);
            void log(char * message, loglevel log_level);
    };
}

simplelogger::logger::logger(uint8_t log_level) {
    _log_level = log_level;
}

void simplelogger::logger::set_log_level(uint8_t log_level) {
    _log_level = log_level;
}

void simplelogger::logger::log(std::string message, loglevel log_level) {
    log(message.c_str(), log_level);
}

void simplelogger::logger::log(char * message, loglevel log_level) {
    if (log_level >= _log_level) {
        time_t rawtime;
        struct tm * timeinfo;

        time(&rawtime);
        timeinfo = localtime(&rawtime);

        if (log_level == SIMPLELOGGER_LEVEL_ERROR) {
            fprintf_s(stderr, "%s: %s", asctime(timeinfo), message);
        }
        else {
            fprintf_s(stdout, "%s: %s", asctime(timeinfo), message);
        }
    }
}