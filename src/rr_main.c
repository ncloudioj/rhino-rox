#include "rr_rhino_rox.h"
#include "rr_logging.h"

#include <stdio.h>

int main(int argc, char *argv[]) {
    UNUSED(argc);
    UNUSED(argv);

    rr_debug("Rhino-Rox is climbing the big tree now...");
    rr_log(LOG_DEBUG, "Rhino is %d years old golden snub-nosed monkey.", 12);
    return 0;
}
