#include <dlt/dlt.h>
#include <sys/time.h>

DLT_DECLARE_CONTEXT(ctx); /* declare context */

int main()
{
	DLT_REGISTER_APP("TAPP", "Test Application for Logging");
	DLT_REGISTER_CONTEXT(ctx, "TES1", "Test Context for Logging");
	
	while (true) {
		DLT_LOG(ctx, DLT_LOG_INFO, DLT_CSTRING("This is a test log"));
		sleep(10);
	}

	DLT_UNREGISTER_CONTEXT(ctx);
	DLT_UNREGISTER_APP();

	return 0;
}
