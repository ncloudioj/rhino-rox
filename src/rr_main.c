#include "rr_rhino_rox.h"
#include "rr_logging.h"
#include "sds.h"
#include "rr_malloc.h"

#include <stdio.h>

int main(int argc, char *argv[]) {
    UNUSED(argc);
    UNUSED(argv);

    char *ss = rr_malloc(10*sizeof(char));

    sds s = sdsnew("hello from sds");
	rr_log(RR_LOG_DEBUG, s);
    rr_log(RR_LOG_DEBUG, "Rhino is %d years old golden snub-nosed monkey.", 12);
    rr_debug("%d Rhino-Rox is climbing the big tree now.", 10);

	size_t used = rr_get_used_memory();
	rr_debug("%zu bytes allocated!", used);
    rr_free(ss);
	sdsfree(s);
    return 0;
}
