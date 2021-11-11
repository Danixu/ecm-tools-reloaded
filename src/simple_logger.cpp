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

#include "simple_logger.h"

simplelogger::logger::logger(loglevel log_level) {
    _log_level = log_level;
}


void simplelogger::logger::set_log_level(loglevel log_level) {
  _log_level = log_level;
}


void simplelogger::logger::log(loglevel log_level, const char * format, ...) {
    va_list args;
    va_start(args, format);
    log(log_level, format, args);
    va_end(args);
}


void simplelogger::logger::log(loglevel log_level, const char * format, va_list args) {
    if (log_level <= _log_level) {
        if (log_level == SIMPLELOGGER_LEVEL_ERROR) {
            fprintf(stderr, "ERROR: ");
            vfprintf(stderr, format, args);
        }
        else {
            switch (log_level)
            {
            case SIMPLELOGGER_LEVEL_INFO:
                fprintf(stderr, "INFO: ");
                break;

            case SIMPLELOGGER_LEVEL_WARNING:
                fprintf(stderr, "WARNING: ");
                break;

            case SIMPLELOGGER_LEVEL_ERROR:
                fprintf(stderr, "ERROR: ");
                break;

            case SIMPLELOGGER_LEVEL_DEBUG:
                fprintf(stderr, "DEBUG: ");
                break;
            
            default:
                break;
            }
            vfprintf(stdout, format, args);
        }
    }
}


void simplelogger::logger::info(const char * format, ...){
    va_list args;
    va_start(args, format);
    log(SIMPLELOGGER_LEVEL_INFO, format, args);
    va_end(args);
}


void simplelogger::logger::warning(const char * format, ...){
    va_list args;
    va_start(args, format);
    log(SIMPLELOGGER_LEVEL_WARNING, format, args);
    va_end(args);
}


void simplelogger::logger::error(const char * format, ...){
    va_list args;
    va_start(args, format);
    log(SIMPLELOGGER_LEVEL_ERROR, format, args);
    va_end(args);
}


void simplelogger::logger::debug(const char * format, ...){
    va_list args;
    va_start(args, format);
    log(SIMPLELOGGER_LEVEL_DEBUG, format, args);
    va_end(args);
}