#include "rr_rhino_rox.h"
#include "rr_logging.h"
#include "jemalloc.h"
#include "sds.h"

#include <stdio.h>

int main(int argc, char *argv[]) {
    UNUSED(argc);
    UNUSED(argv);

    char *ss = je_malloc(10*sizeof(char));
    je_free(ss);

    sds s = sdsnew("hello");

	rr_log(RR_LOG_DEBUG, s);
	sdsfree(s);
    rr_debug("%d Rhino-Rox is climbing the big tree now.", 10);
    rr_log(RR_LOG_DEBUG, "Rhino is %d years old golden snub-nosed monkey.", 12);
    return 0;
}
