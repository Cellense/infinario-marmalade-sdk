#ifndef INFINARIO_TEST_H
#define INFINARIO_TEST_H

#include "../src/Infinario.h"

#include "s3e.h"
#include <sstream>

class Test
{
public:
	enum class State : char
	{
		Running = 0,
		Succeeded = 1,
		Failed = 2
	};

	virtual ~Test();

	virtual void Init() = 0;
	virtual void Update() = 0;
	virtual void Terminate() = 0;

	bool IsComplete() const;
	void Render(const int32 x, const int32 y, const int32 id) const;
	void OutputLog(const int32 id) const;
protected:
	std::stringstream log;

	virtual State GetState() const = 0;
};

#endif // INFINARIO_TEST_H