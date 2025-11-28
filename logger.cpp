#include "logger.hpp"

LazyConstruction<Logger, std::string> global_logger("global_logger");
