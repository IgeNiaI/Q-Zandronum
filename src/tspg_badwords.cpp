#include <cctype>
#include <errno.h>
#include "tspg_badwords.h"
#include "tarray.h"
#include "zstring.h"
#include "c_cvars.h"
#include "i_system.h"
#include "sc_man.h"
#include "v_text.h"

// Replacement for FScanner::MustGetString that doesn't fatally error
// This is an ugly hack
#define badwords_ScriptError(fmt, ...)																	\
	Printf("%s: ", __FUNCTION__);																		\
	Printf(fmt, ##__VA_ARGS__);																			\
	Printf("\n");																						\
	Printf("%s:     ... at %s line %d\n", __FUNCTION__, filename, sc.Line);								\
	if (includedFrom)																					\
		Printf("%s:     ... included from %s line %d\n", __FUNCTION__, includedFrom, includeLine);		\
	return false;

#define badwords_MustGetString()																		\
	if (sc.GetString() == false) {																		\
		badwords_ScriptError("Missing string (unexpected end of file).");								\
	}

#define badwords_Print(fmt, ...)																			\
	if (tspg_badwords_debug) {																			\
		Printf(fmt, ##__VA_ARGS__);																		\
	}

enum class BadWordType
{
	Word,
	Includes
};

struct BadWordEntry
{
	BadWordType type;
	FString word;
};

CVAR(String, tspg_badwordsfile, "", TSPG_NOSET);
CVAR(Bool, tspg_badwords_debug, false, TSPG_NOSET);
CUSTOM_CVAR(Int, tspg_badwordsreloadtime, 600, TSPG_NOSET)
{
	if (self.GetGenericRep(CVAR_Int).Int < 1)
		self = 1;
}

CUSTOM_CVAR(Bool, tspg_badwordsenabled, false, TSPG_NOSET)
{
	if (self)
		BADWORDS_Init();
	else
		BADWORDS_Shutdown();
}

static TArray<BadWordEntry> BadWords;

static bool badwords_Load();
static bool badwords_LoadFile(const char *filename, const char *includedFrom = nullptr, int includeLine = -1);
static void badwords_Clear();
static bool badwords_CheckStringEquals(FString text);
static bool badwords_CheckStringWords(FString text);
static bool badwords_CheckStringIncludes(FString text);

static unsigned lastReload = 0;

void BADWORDS_Init()
{
	if (!tspg_badwordsenabled)
		return;

	badwords_Load();
}

void BADWORDS_Shutdown()
{
	if (!tspg_badwordsenabled)
		return;

	badwords_Clear();
}

bool badwords_Load()
{
	if (BadWords.Size() > 0)
	{
		badwords_Clear();
	}

	// load the file
	const char *filename = tspg_badwordsfile.GetGenericRep(CVAR_String).String;

	if (!badwords_LoadFile(filename))
	{
		Printf("\\cgbadwords_Load: Failed to load bad words file %s\n", filename);
		return false;
	}

	badwords_Print("badwords_Load: Loaded %d entries\n", BadWords.Size());

	lastReload = I_MSTime();

	return true;
}

bool badwords_LoadFile(const char *filename, const char *includedFrom, int includeLine)
{
	// check if we can read the file first
	FILE *file;
	if ((file = fopen(filename, "r")) == nullptr)
	{
		Printf("\\cgbadwords_LoadFile: Failed to open file for read: %s (%d: %s)\n", filename, errno, strerror(errno));
		if (includedFrom)
			Printf("\\cgbadwords_LoadFile:    \\cg... included from %s line %d\n", includedFrom, includeLine);
		return false;
	}

	fclose(file);

	// parse the file
	FScanner sc;
	sc.OpenFile(filename);
	while (sc.GetString())
	{
		if (sc.Compare("include"))
		{
			badwords_MustGetString();

			const char *next = sc.String;
			if (!badwords_LoadFile(next, filename, sc.Line))
				return false;
		}

		else if (sc.Compare("word"))
		{
			badwords_MustGetString();
			FString word(sc.String);

			BadWordEntry entry;
			entry.type = BadWordType::Word;
			entry.word = word;

			BadWords.Push(entry);
		}

		else if (sc.Compare("contains"))
		{
			badwords_MustGetString();
			FString word(sc.String);

			BadWordEntry entry;
			entry.type = BadWordType::Includes;
			entry.word = word;

			BadWords.Push(entry);
		}

		else
		{
			badwords_ScriptError("Unknown command \"%s\"", sc.String);
		}
	}

	return true;
}

void BADWORDS_AttemptReload()
{
	if (!tspg_badwordsenabled)
		return;

	unsigned now = I_MSTime();
	unsigned reloadtime = tspg_badwordsreloadtime * 1000 * 60;

	if (now - lastReload >= reloadtime)
	{
		badwords_Print("BADWORDS_AttemptReload: Attempting reload.\n");
		badwords_Load();
	}
}

void badwords_Clear() {
	BadWords.Clear();
}

bool badwords_CheckStringEquals(FString text)
{
	FString buffer;

	// Make a version of the text with only alphanumeric chars
	for (size_t i = 0; i < text.Len(); i++)
	{
		char c = text[i];

		if (!isalnum(c)) {
			continue;
		}

		buffer.AppendFormat("%c", c);
	}

	// Now check the words
	for (size_t i = 0; i < BadWords.Size(); i++)
	{
		BadWordEntry *entry = &BadWords[i];

		if (entry->type != BadWordType::Word)
		{
			continue;
		}

		if (entry->word.CompareNoCase(buffer) == 0)
		{
			badwords_Print("badwords_CheckStringEquals: Got a match for \"%s\".\n", entry->word.GetChars());
			return true;
		}
	}

	return false;
}

static bool badwords_CheckStringWords(FString text)
{
	TArray<FString> words;
	FString buffer;

	for (size_t i = 0; i < text.Len(); i++)
	{
		char c = text[i];

		// If we hit a space, push it to the array
		if (c == ' ')
		{
			if (buffer.Len() >= 1)
			{
				words.Push(buffer);
				buffer = "";
			}

			continue;
		}

		// Ignore it if it's not alphanumeric
		if (!isalnum(c)) 
		{
			continue;
		}

		// Add it to the buffer
		buffer.AppendFormat("%c", c);
	}
	
	// Push anything left over in the buffer
	if (buffer.Len() >= 1)
	{
		words.Push(buffer);
		buffer = "";
	}

	// Now check the words
	for (size_t i = 0; i < BadWords.Size(); i++) {
		BadWordEntry *entry = &BadWords[i];

		if (entry->type != BadWordType::Word)
		{
			continue;
		}

		for (size_t j = 0; j < words.Size(); j++)
		{
			FString word = words[j];

			if (entry->word.CompareNoCase(word) == 0)
			{
				badwords_Print("badwords_CheckStringWords: Got a match for \"%s\".\n", entry->word.GetChars());
				return true;
			}
		}
	}

	// We found nothing
	return false;
}

bool badwords_CheckStringIncludes(FString text)
{
	FString lowerText(text);
	lowerText.ToLower();

	// Now check the words
	for (size_t i = 0; i < BadWords.Size(); i++) {
		BadWordEntry *entry = &BadWords[i];

		if (entry->type != BadWordType::Includes)
		{
			continue;
		}

		FString lowerWord(entry->word);
		lowerWord.ToLower();

		if (lowerText.IndexOf(lowerWord) != -1)
		{
			badwords_Print("badwords_CheckStringIncludes: Got a match for \"%s\".\n", entry->word.GetChars());
			return true;
		}
	}

	return false;
}

bool BADWORDS_ShouldFilter(const char *text)
{
	if (!tspg_badwordsenabled) {
		return false;
	}

	FString str(text);
	V_RemoveColorCodes(str);

	if (badwords_CheckStringEquals(str))
		return true;

	if (badwords_CheckStringIncludes(str))
		return true;

	if (badwords_CheckStringWords(str))
		return true;

	return false;
}
