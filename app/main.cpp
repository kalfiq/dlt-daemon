#include <dlt/dlt.h>

DLT_DECLARE_CONTEXT(ctx); /* declare context */

int main()
{
	DLT_REGISTER_APP("TAPP", "Test Application for Logging");

	DLT_REGISTER_CONTEXT(ctx, "TES1", "Test Context for Logging");

	/* … */

	DLT_LOG(ctx, DLT_LOG_ERROR, DLT_CSTRING("This is an error"));

	/* … */

	DLT_UNREGISTER_CONTEXT(ctx);
	DLT_UNREGISTER_APP();
	return 0;
}
