#ifndef TEXT_H_INCLUDED
#define TEXT_H_INCLUDED

//messages, used only in server
const char *ERROR_DIMENSIONS = "ERROR Width and height must be positive.";
const char *ERROR_WORKERS_COUNT = "ERROR The field cannot be divided to this amount of workers.";
const char *CORRECT_USE_INFO = "Correct use:\n./life-client [width] [height] [workers_count].";

const char *LOG_COMMAND_RECIEVED = "Command recieved:";

//messages, which will be sended to client
const char *ERROR_NO = "OK";
const char *ERROR_UNKNOWN = "ERROR Unknown command.";
const char *ERROR_NOT_SUPPORTED = "ERROR Not supported yet.";
const char *ERROR_TOO_FEW_ARGS = "ERROR Too few arguments.";
const char *ERROR_TOO_MUCH_ARGS = "ERROR Too much arguments.";
const char *ERROR_NUMERIC_ARG = "ERROR Failed to convert argument to number";
const char *ERROR_COORDINATES = "ERROR Wrong coordinates.";
const char *ERROR_WRONG_GEN = "ERROR Generation has already reached.";
const char *ERROR_NOT_STARTED = "ERROR Nothing to stop.";
const char *ERROR_FILE_OPEN = "ERROR File is not exists or access violation.";
const char *ERROR_FILE_FORMAT = "ERROR Wrong file format.";
const char *ERROR_FILE_CREATE = "ERROR Fail to create file.";

#endif //TEXT_H_INCLUDED
