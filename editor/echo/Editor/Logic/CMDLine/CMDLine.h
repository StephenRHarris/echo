#pragma once

#include <engine/core/util/StringUtil.h>

namespace Echo
{
	/**
	 * Command Parser 2012-8-16 Liang
	 */
	class CMDLine
	{
	public:
		// Parse
		static bool Parser(int argc, char* argv[]);
	};

	/**
	 * EditorMode
	 */
	class EditorMode
	{
	public:
		// exec command
		bool exec(int argc, char* argv[]);
	};

	// EditOpen
	class EditOpenMode
	{
	public:
		// exec command
		bool exec(int argc, char* argv[]);
	};

	/**
	 * GameMode
	 */
	class GameMode
	{
	public:
		// exec command
		bool exec(int argc, char* argv[]);
	};
}