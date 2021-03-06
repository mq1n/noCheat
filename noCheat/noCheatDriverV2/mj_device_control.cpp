/************************************************************************/
/* IRP_MJ_DEVICE_CONTROL                                                */
/************************************************************************/

// Include DDK
#include <ntddk.h>

// Include driver headers
#include "defines.h"
#include "globals.h"
#include "ncDriverDefines.h"
#include "link.h"

// Define our procedure for mapping
#define MAP_LINK(name, src, dest, size) if(src > 0){ LOG2("Mapping space for: " name); TryMapLink((void*)src, &dest, &returnInf, size); }

// Define a macro for size assertions

#define CHECK_NC_SIZE(a, b) NASSERT((a == sizeof(struct b)), {returnInf.bSizeMismatch = 1; goto WriteReturn;})

/*
 * Called to initiate a link between the driver and a service
 */
extern "C" NTSTATUS DrvDevLink(IN PDEVICE_OBJECT device, IN PIRP Irp)
{
	// Unreferenced Param
	UNREFERENCED_PARAMETER(device);

	// Log
	LOG("Initiating link");

	// Init return char
	char ret;

	// Get current stack location
	PIO_STACK_LOCATION pLoc = IoGetCurrentIrpStackLocation(Irp);
	
	// Setup input info
	NC_CONNECT_INFO_INPUT* inputInf = (NC_CONNECT_INFO_INPUT*)Irp->AssociatedIrp.SystemBuffer;

	// Switch code (intent)
	switch(pLoc->Parameters.DeviceIoControl.IoControlCode)
	{
	case NC_CONNECTION_CODE:
		// Log
		LOG2("Connection wants to map containers");

		// Assert size
		NASSERT (pLoc->Parameters.DeviceIoControl.InputBufferLength == sizeof(NC_CONNECT_INFO_INPUT), goto WriteReturn);

		// Setup output info
		NC_CONNECT_INFO_OUTPUT returnInf;

		// Check size of return struct
		CHECK_NC_SIZE(inputInf->iReturnSize, NC_CONNECT_INFO_OUTPUT);

		// Attempt to map return value
		MAP_LINK("Return", inputInf->pReturnInfo, sSpaces.Return, inputInf->iReturnSize);

		// Check for existing connection
		if(sSpaces.bLink == 1)
		{
			// Set blocked and non-success
			returnInf.bBlocked = 1;
			returnInf.bSuccess = 0;

			// Write and quit
			goto WriteReturn;
		}

		// Check sizes
		CHECK_NC_SIZE(inputInf->iImageEventSize, NC_IMAGE_EVENT);
		CHECK_NC_SIZE(inputInf->iProcessEventSize, NC_PROCESS_EVENT);
		CHECK_NC_SIZE(inputInf->iProcessContainerSize, NC_PROCESS_CONTAINER);
		CHECK_NC_SIZE(inputInf->iImageContainerSize, NC_IMAGE_CONTAINER);
		CHECK_NC_SIZE(inputInf->iThreadContainerSize, NC_THREAD_CONTAINER);
		CHECK_NC_SIZE(inputInf->iThreadEventSize, NC_THREAD_EVENT);

		// Verify link
		ret = VerifyLink(inputInf);

		// Test Verification
		if(ret == 1)
			LOG("Successfully validated image receiver!");
		else
		{
			LOG("Could not validate image receiver!");
			returnInf.bAccessDenied = 1;
			returnInf.bSuccess = 0;
			goto WriteReturn;
		}

		// Try to map links
		MAP_LINK("Images", inputInf->pImageLoadContainer, sSpaces.Images, inputInf->iImageContainerSize);
		MAP_LINK("Processes", inputInf->pProcessCreateContainer, sSpaces.Processes, inputInf->iProcessContainerSize);
		MAP_LINK("Threads", inputInf->pThreadCreateContainer, sSpaces.Threads, inputInf->iThreadContainerSize);

		// Check sizes flag
		if(returnInf.bSizeMismatch == 1)
		{
			// Log
			LOG("Size mismatch! Returning failure!");

			// Close all links
			CloseLinks();

			// End
			break;
		}

		// Signal a success
		returnInf.bSuccess = 1;

		// Signal that we have a link
		sSpaces.bLink = 1;

WriteReturn:
		// Log
		LOG3("Writing return information");

		// Check/write return information
		if(sSpaces.Return.bMapped == 1)
			memcpy(sSpaces.Return.oContainer, &returnInf, sizeof(NC_CONNECT_INFO_OUTPUT));

		break;
	default:
		// Log
		LOG("Control connection type not implemented/supported, or an old code has been used (%d)", pLoc->Parameters.DeviceIoControl.IoControlCode);
	}

	// Log
	LOG2("Completing link initiation");

	// Complete request
	Irp->IoStatus.Status = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	// Return success
	return STATUS_SUCCESS;
}