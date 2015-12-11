#include "Test.h"

#include "IwGx.h"

#include "s3e.h"
#include "s3eKeyboard.h"

#include <vector>

#define FRAME_RATE 25

void CreateTests(std::vector<Test *> &tests);
void DestroyTests(std::vector<Test *> &tests);

int main()
{
	const int32 msPerFrame = 1000 / FRAME_RATE;

	const int32 renderBorder = 15;
	const int32 renderHeight = 25;

	IwGxInit();

	std::vector<Test *>* tests = new std::vector<Test *>();
	CreateTests(*tests);
	
	// Init Tests.
	for (std::vector<Test *>::iterator it = tests->begin(), end = tests->end(); it != end; ++it) {
		(*it)->Init();
	}

	// Set screen clear colour
	IwGxSetColClear(0xff, 0xff, 0xff, 0xff);
	IwGxPrintSetScale(2);

	for (;;) {
		s3eDeviceYield(0);
		s3eKeyboardUpdate();

		int64 start = s3eTimerGetMs();

		// Update Tests.
		for (std::vector<Test *>::iterator it = tests->begin(), end = tests->end(); it != end; ++it) {
			(*it)->Update();
		}

		// Check tests state.
		bool testsComplete = true;
		for (std::vector<Test *>::iterator it = tests->begin(), end = tests->end(); it != end; ++it) {
			if (!(*it)->IsComplete()) {
				testsComplete = false;
				break;
			}
		}

		// Test if exit is possible.
		if ((testsComplete && (s3eKeyboardGetState(s3eKeyEsc) & S3E_KEY_STATE_DOWN)) || s3eDeviceCheckQuitRequest()) {
			break;
		}

		// Clear the screen.
		IwGxClear(IW_GX_COLOUR_BUFFER_F | IW_GX_DEPTH_BUFFER_F);

		// Render Tests.
		for (int32 i = 0, count = static_cast<int32>(tests->size()); i < count; ++i) {
			(*tests)[i]->Render(5 * renderBorder, 3 * renderBorder + i * (renderBorder + renderHeight), i + 1);
		}
		
		// Output exit message.
		if (testsComplete) {
			int32 x = 3 * renderBorder;
			int32 y = 4 * renderBorder + static_cast<int32>(tests->size()) * (renderBorder + renderHeight);
			IwGxPrintString(x, y, "`x666666""All tests finished successfully.", false);
			IwGxPrintString(x, y + (renderBorder + renderHeight), "`x666666""Press the Esc key to exit.", false);
		}

		// Swap buffers
		IwGxFlush();
		IwGxSwapBuffers();

		// Attempt to lock frame rate.
		while ((s3eTimerGetMs() - start) < msPerFrame) {
			int32 yield = msPerFrame - static_cast<int32>(s3eTimerGetMs() - start);
			if (yield < 0) {
				break;
			}
			s3eDeviceYield(yield);
		}
	}

	// Terminate Tests.
	for (std::vector<Test *>::iterator it = tests->begin(), end = tests->end(); it != end; ++it) {
		(*it)->Terminate();
	}

	// Output Test Logs.
	for (int32 i = 0, count = static_cast<int32>(tests->size()); i < count; ++i) {
		(*tests)[i]->OutputLog(i + 1);
	}

	DestroyTests(*tests);
	delete tests;
	
	IwGxTerminate();

	return 0;
}
