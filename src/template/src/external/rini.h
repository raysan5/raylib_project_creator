/**********************************************************************************************
*
*   rini v1.0 - A simple and easy-to-use config init files reader and writer
*
*   DESCRIPTION:
*       Load and save config init properties
*
*   FEATURES:
*       - Config files reading and writing
*       - Supported value types: int, string
*       - Support custom line comment character
*       - Support custom property delimiters
*       - Support inline comments with custom delimiter
*       - Support multi-word text values w/o quote delimiters
*       - Minimal C standard lib dependency (optional)
*       - Customizable maximum config values capacity
*
*   LIMITATIONS:
*       - Config [sections] not supported
*       - Saving config file requires complete rewrite
*
*   POSSIBLE IMPROVEMENTS:
*       - Support config [sections]
*       - Support disabled entries recognition
*       - Support comment lines and empty lines
*
*   CONFIGURATION:
*       #define RINI_IMPLEMENTATION
*           Generates the implementation of the library into the included file.
*           If not defined, the library is in header only mode and can be included in other headers
*           or source files without problems. But only ONE file should hold the implementation.
*
*       #define RINI_MAX_LINE_SIZE
*           Defines the maximum size of line buffer to read from file.
*           Default value: 512 bytes (considering [key + text + desc] is 256 max size by default)
*
*       #define RINI_MAX_KEY_SIZE
*           Defines the maximum size of value key
*           Default value: 64 bytes
*
*       #define RINI_MAX_TEXT_SIZE
*           Defines the maximum size of value text
*           Default value: 64 bytes
*
*       #define RINI_MAX_DESC_SIZE
*           Defines the maximum size of value description
*           Default value: 128 bytes
*
*       #define RINI_MAX_VALUE_CAPACITY
*           Defines the maximum number of values supported
*           Default value: 32 entries support
*
*       #define RINI_VALUE_DELIMITER
*           Defines a key value delimiter, in case it is defined.
*           Most .ini files use '=' as value delimiter but ':' is also used or just whitespace
*           Default value: ' '
*
*       #define RINI_LINE_COMMENT_DELIMITER
*           Define character used to comment lines, placed at beginning of line
*           Most .ini files use semicolon ';' but '#' is also used
*           Default value: '#'
*
*       #define RINI_VALUE_COMMENTS_DELIMITER
*           Defines a property line-end comment delimiter
*           This implementation allows adding inline comments after the value.
*           Default value: '#'
*
*       #define RINI_LINE_SECTION_DELIMITER
*           Defines section lines start character
*           Sections loading is not supported, lines are just skipped for now
*
*   DEPENDENCIES: C standard library:
*       - stdio.h: fopen(), feof(), fgets(), fclose(), fprintf()
*       - stdlib.h: malloc(), calloc(), free()
*       - string.h: memset(), memcpy(), strcmp(), strlen()
*
*   VERSIONS HISTORY:
*       1.0 (18-May-2023) First release, basic read/write functionality
*
*
*   LICENSE: zlib/libpng
*
*   Copyright (c) 2023 Ramon Santamaria (@raysan5)
*
*   This software is provided "as-is", without any express or implied warranty. In no event
*   will the authors be held liable for any damages arising from the use of this software.
*
*   Permission is granted to anyone to use this software for any purpose, including commercial
*   applications, and to alter it and redistribute it freely, subject to the following restrictions:
*
*     1. The origin of this software must not be misrepresented; you must not claim that you
*     wrote the original software. If you use this software in a product, an acknowledgment
*     in the product documentation would be appreciated but is not required.
*
*     2. Altered source versions must be plainly marked as such, and must not be misrepresented
*     as being the original software.
*
*     3. This notice may not be removed or altered from any source distribution.
*
**********************************************************************************************/

#ifndef RINI_H
#define RINI_H

#define RINI_VERSION    "1.0"

// Function specifiers in case library is build/used as a shared library (Windows)
// NOTE: Microsoft specifiers to tell compiler that symbols are imported/exported from a .dll
#if defined(_WIN32)
    #if defined(BUILD_LIBTYPE_SHARED)
        #define RINIAPI __declspec(dllexport)     // We are building the library as a Win32 shared library (.dll)
    #elif defined(USE_LIBTYPE_SHARED)
        #define RINIAPI __declspec(dllimport)     // We are using the library as a Win32 shared library (.dll)
    #endif
#endif

// Function specifiers definition
#ifndef RINIAPI
    #define RINIAPI       // Functions defined as 'extern' by default (implicit specifiers)
#endif

//----------------------------------------------------------------------------------
// Defines and Macros
//----------------------------------------------------------------------------------
// Allow custom memory allocators
#ifndef RINI_MALLOC
    #define RINI_MALLOC(sz)       malloc(sz)
#endif
#ifndef RINI_CALLOC
    #define RINI_CALLOC(n,sz)     calloc(n,sz)
#endif
#ifndef RINI_FREE
    #define RINI_FREE(p)          free(p)
#endif

// Simple log system to avoid printf() calls if required
// NOTE: Avoiding those calls, also avoids const strings memory usage
#define RINI_SUPPORT_LOG_INFO
#if defined(RINI_SUPPORT_LOG_INFO)
  #define RINI_LOG(...)           printf(__VA_ARGS__)
#else
  #define RINI_LOG(...)
#endif

#if !defined(RINI_MAX_LINE_SIZE)
    #define RINI_MAX_LINE_SIZE              512
#endif

#if !defined(RINI_MAX_KEY_SIZE)
    #define RINI_MAX_KEY_SIZE                64
#endif

#if !defined(RINI_MAX_TEXT_SIZE)
    #define RINI_MAX_TEXT_SIZE               64
#endif

#if !defined(RINI_MAX_DESC_SIZE)
    #define RINI_MAX_DESC_SIZE              128
#endif

#if !defined(RINI_MAX_VALUE_CAPACITY)
    #define RINI_MAX_VALUE_CAPACITY          32
#endif

#if !defined(RINI_VALUE_DELIMITER)
    #define RINI_VALUE_DELIMITER            ' '
#endif
#if !defined(RINI_LINE_COMMENT_DELIMITER)
    #define RINI_LINE_COMMENT_DELIMITER     '#'
#endif
#if !defined(RINI_VALUE_COMMENTS_DELIMITER)
    #define RINI_VALUE_COMMENTS_DELIMITER   '#'
#endif
#if !defined(RINI_LINE_SECTION_DELIMITER)
    #define RINI_LINE_SECTION_DELIMITER     '['
#endif

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
// Config value entry
typedef struct {
    char key[RINI_MAX_KEY_SIZE];    // Config value key identifier
    char text[RINI_MAX_TEXT_SIZE];  // Config value text
    char desc[RINI_MAX_DESC_SIZE];  // Config value description
} rini_config_value;

// Config data
typedef struct {
    rini_config_value *values;      // Config values array
    unsigned int count;             // Config values count
    unsigned int capacity;          // Config values capacity
} rini_config;

#if defined(__cplusplus)
extern "C" {                    // Prevents name mangling of functions
#endif

//------------------------------------------------------------------------------------
// Functions declaration
//------------------------------------------------------------------------------------
RINIAPI rini_config rini_load_config(const char *file_name);            // Load config from file (*.ini) or create a new config object (pass NULL)
RINIAPI rini_config rini_load_config_from_memory(const char *text);     // Load config from text buffer
RINIAPI void rini_save_config(rini_config config, const char *file_name, const char *header); // Save config to file, with custom header
RINIAPI char *rini_save_config_to_memory(rini_config config, const char *header); // Save config to text buffer ('\0' EOL)
RINIAPI void rini_unload_config(rini_config *config);                   // Unload config data from memory

RINIAPI int rini_get_config_value(rini_config config, const char *key); // Get config value int for provided key, returns 0 if not found
RINIAPI const char *rini_get_config_value_text(rini_config config, const char *key); // Get config value text for provided key
RINIAPI const char *rini_get_config_value_description(rini_config config, const char *key); // Get config value description for provided key

RINIAPI int rini_get_config_value_fallback(rini_config config, const char *key, int fallback); // Get config value for provided key with default value fallback if not found or not valid
RINIAPI const char *rini_get_config_value_text_fallback(rini_config config, const char *key, const char *fallback); // Get config value text for provided key with fallback if not found or not valid

// Set config value int/text and description for existing key or create a new entry
// NOTE: When setting a text value, if id does not exist, a new entry is automatically created
RINIAPI int rini_set_config_value(rini_config *config, const char *key, int value, const char *desc);
RINIAPI int rini_set_config_value_text(rini_config *config, const char *key, const char *text, const char *desc);

// Set config value description for existing key
// WARNING: Key must exist to add description, if a description exists, it is updated
RINIAPI int rini_set_config_value_description(rini_config *config, const char *key, const char *desc);

#ifdef __cplusplus
}
#endif

#endif // RINI_H

/***********************************************************************************
*
*   RINI IMPLEMENTATION
*
************************************************************************************/

#if defined(RINI_IMPLEMENTATION)

#include <stdio.h>          // Required for: fopen(), feof(), fgets(), fclose(), fprintf()
#include <stdlib.h>         // Required for: malloc(), calloc(), free()
#include <string.h>         // Required for: memset(), memcpy(), strcmp(), strlen()

//----------------------------------------------------------------------------------
// Defines and macros
//----------------------------------------------------------------------------------
// ...

//----------------------------------------------------------------------------------
// Global variables definition
//----------------------------------------------------------------------------------
// ...

//----------------------------------------------------------------------------------
// Module internal functions declaration
//----------------------------------------------------------------------------------
static int rini_read_config_key(const char *buffer, char *key); // Get key from a buffer line containing key-value-(description)
static int rini_read_config_value_text(const char *buffer, char *text, char *desc); // Get value text (and description) from a buffer line

static int rini_text_to_int(const char *text); // Convert text to int value (if possible), same as atoi()

//----------------------------------------------------------------------------------
// Module functions declaration
//----------------------------------------------------------------------------------
// Load config from file (.ini)
rini_config rini_load_config(const char *file_name)
{
    rini_config config = { 0 };
    unsigned int value_counter = 0;

    // Init config data to max capacity
    config.capacity = RINI_MAX_VALUE_CAPACITY;
    config.values = (rini_config_value *)RINI_CALLOC(RINI_MAX_VALUE_CAPACITY, sizeof(rini_config_value));

    if (file_name != NULL)
    {
        FILE *rini_file = fopen(file_name, "rt");

        if (rini_file != NULL)
        {
            char buffer[RINI_MAX_LINE_SIZE] = { 0 };    // Buffer to read every text line

            // First pass to count valid config lines
            while (!feof(rini_file))
            {
                // WARNING: fgets() keeps line endings, doesn't have any special options for converting line endings,
                // but on Windows, when reading file 'rt', line endings are converted from \r\n to just \n
                fgets(buffer, RINI_MAX_LINE_SIZE, rini_file);

                // Skip commented lines and empty lines
                // NOTE: We are also skipping sections delimiters
                if ((buffer[0] != RINI_LINE_COMMENT_DELIMITER) &&
                    (buffer[0] != RINI_LINE_SECTION_DELIMITER) &&
                    (buffer[0] != '\n') && (buffer[0] != '\0')) value_counter++;
            }

            // WARNING: We can't store more values than its max capacity
            config.count = (value_counter > RINI_MAX_VALUE_CAPACITY)? RINI_MAX_VALUE_CAPACITY : value_counter;

            if (config.count > 0)
            {
                rewind(rini_file);
                value_counter = 0;

                // Second pass to read config data
                while (!feof(rini_file))
                {
                    // WARNING: fgets() keeps line endings, doesn't have any special options for converting line endings,
                    // but on Windows, when reading file 'rt', line endings are converted from \r\n to just \n
                    fgets(buffer, RINI_MAX_LINE_SIZE, rini_file);

                    // Skip commented lines and empty lines
                    if ((buffer[0] != '#') && (buffer[0] != ';') && (buffer[0] != '\n') && (buffer[0] != '\0'))
                    {
                        // Get keyentifier string
                        memset(config.values[value_counter].key, 0, RINI_MAX_KEY_SIZE);
                        rini_read_config_key(buffer, config.values[value_counter].key);
                        rini_read_config_value_text(buffer, config.values[value_counter].text, config.values[value_counter].desc);

                        value_counter++;
                    }
                }
            }

            fclose(rini_file);
        }
    }

    return config;
}

// Load config from text buffer
rini_config rini_load_config_from_memory(const char *text)
{
    #define RINI_MAX_TEXT_LINES RINI_MAX_VALUE_CAPACITY*2       // Consider possible comments and empty lines

    rini_config config = { 0 };
    unsigned int value_counter = 0;

    // Init config data to max capacity
    config.capacity = RINI_MAX_VALUE_CAPACITY;
    config.values = (rini_config_value *)RINI_CALLOC(RINI_MAX_VALUE_CAPACITY, sizeof(rini_config_value));

    if (text != NULL)
    {
        // Split text by line-breaks
        const char *lines[RINI_MAX_TEXT_LINES] = { 0 };
        int textSize = (int)strlen(text);
        lines[0] = text;
        int line_counter = 1;

        for (int i = 0, k = 1; (i < textSize) && (line_counter < RINI_MAX_TEXT_LINES); i++)
        {
            if (text[i] == '\n')
            {
                lines[k] = &text[i + 1]; // WARNING: next value is valid?
                line_counter += 1;
                k++;
            }
        }

        // Count possible values in lines
        for (int l = 0; l < line_counter; l++)
        {
            // Skip commented lines and empty lines
            // NOTE: We are also skipping sections delimiters
            if ((lines[l][0] != RINI_LINE_COMMENT_DELIMITER) &&
                (lines[l][0] != RINI_LINE_SECTION_DELIMITER) &&
                (lines[l][0] != '\n') && (lines[l][0] != '\0')) value_counter++;
        }

        // WARNING: We can't store more values than its max capacity
        config.count = (value_counter > RINI_MAX_VALUE_CAPACITY)? RINI_MAX_VALUE_CAPACITY : value_counter;

        // Process lines to get keys and values
        if (config.count > 0)
        {
            value_counter = 0;

            // Second pass to read config data
            for (int l = 0; l < line_counter; l++)
            {
                // Skip commented lines and empty lines
                if ((lines[l][0] != '#') && (lines[l][0] != ';') && (lines[l][0] != '\n') && (lines[l][0] != '\0'))
                {
                    // Get keyentifier string
                    memset(config.values[value_counter].key, 0, RINI_MAX_KEY_SIZE);
                    rini_read_config_key(lines[l], config.values[value_counter].key);
                    rini_read_config_value_text(lines[l], config.values[value_counter].text, config.values[value_counter].desc);

                    value_counter++;
                }
            }
        }
    }

    return config;
}

// Save config to file (*.ini)
void rini_save_config(rini_config config, const char *file_name, const char *header)
{
    FILE *rini_file = fopen(file_name, "wt");

    if (rini_file != NULL)
    {
        if (header != NULL) fprintf(rini_file, "%s", header);

        for (unsigned int i = 0; i < config.count; i++)
        {
            // TODO: If text is not a number value, append text-quotes?

            fprintf(rini_file, "%-22s %c %6s      %c %s\n", config.values[i].key, RINI_VALUE_DELIMITER, config.values[i].text, RINI_VALUE_COMMENTS_DELIMITER, config.values[i].desc);
        }

        fclose(rini_file);
    }
}

// Save config to text buffer ('\0' EOL)
char *rini_save_config_to_memory(rini_config config, const char *header)
{
    #define RINI_MAX_TEXT_FILE_SIZE  512

    static char text[RINI_MAX_TEXT_FILE_SIZE] = { 0 };
    memset(text, 0, RINI_MAX_TEXT_FILE_SIZE);
    int offset = 0;

    if (header != NULL)
    {
        offset = sprintf(text, "%s", header);
    }

    for (unsigned int i = 0; i < config.count; i++)
    {
        // TODO: If text is not a number value, append text-quotes?
        offset += sprintf(text + offset, "%-22s %c %6s      %c %s\n", config.values[i].key, RINI_VALUE_DELIMITER, config.values[i].text, RINI_VALUE_COMMENTS_DELIMITER, config.values[i].desc);
    }

    return text;
}

// Unload config data
void rini_unload_config(rini_config *config)
{
    RINI_FREE(config->values);

    config->values = NULL;
    config->count = 0;
    config->capacity = 0;
}

// Get config value for provided key, returns 0 if not found or not valid
int rini_get_config_value(rini_config config, const char *key)
{
    int value = 0;

    for (unsigned int i = 0; i < config.count; i++)
    {
        if (strcmp(key, config.values[i].key) == 0)
        {
            value = rini_text_to_int(config.values[i].text);
            break;
        }
    }

    return value;
}

// Get config value for provided key with default value fallback if not found or not valid
int rini_get_config_value_fallback(rini_config config, const char *key, int fallback)
{
    int value = fallback;

    for (unsigned int i = 0; i < config.count; i++)
    {
        if (strcmp(key, config.values[i].key) == 0) // Key found
        {
            // TODO: Detect if conversion fails...
            value = rini_text_to_int(config.values[i].text);
            break;
        }
    }

    return value;
}

// Get config text for string id
const char *rini_get_config_value_text(rini_config config, const char *key)
{
    const char *text = NULL;

    for (unsigned int i = 0; i < config.count; i++)
    {
        if (strcmp(key, config.values[i].key) == 0)
        {
            text = config.values[i].text;
            break;
        }
    }

    return text;
}

// Get config value text for provided key with fallback if not found or not valid
RINIAPI const char *rini_get_config_value_text_fallback(rini_config config, const char *key, const char *fallback)
{
    const char *text = fallback;

    for (unsigned int i = 0; i < config.count; i++)
    {
        if (strcmp(key, config.values[i].key) == 0) // Key found
        {
            text = config.values[i].text;
            break;
        }
    }

    return text;
}

// Get config description for string id
const char *rini_get_config_value_description(rini_config config, const char *key)
{
    const char *desc = NULL;

    for (unsigned int i = 0; i < config.count; i++)
    {
        if (strcmp(key, config.values[i].key) == 0)
        {
            desc = config.values[i].desc;
            break;
        }
    }

    return desc;
}

// Set config value and description for existing key or create a new entry
int rini_set_config_value(rini_config *config, const char *key, int value, const char *desc)
{
    int result = -1;
    char value_text[RINI_MAX_TEXT_SIZE] = { 0 };

    sprintf(value_text, "%i", value);

    result = rini_set_config_value_text(config, key, value_text, desc);

    return result;
}

// Set config value text and description for existing key or create a new entry
// NOTE: When setting a text value, if id does not exist, a new entry is automatically created
int rini_set_config_value_text(rini_config *config, const char *key, const char *text, const char *desc)
{
    int result = -1;

    if ((text == NULL) || (text[0] == '\0')) return result;

    // Try to find key and update text and description
    for (unsigned int i = 0; i < config->count; i++)
    {
        if (strcmp(key, config->values[i].key) == 0)
        {
            memset(config->values[i].text, 0, RINI_MAX_TEXT_SIZE);
            memcpy(config->values[i].text, text, strlen(text));

            memset(config->values[i].desc, 0, RINI_MAX_DESC_SIZE);
            if (desc != NULL) memcpy(config->values[i].desc, desc, strlen(desc));
            result = 0;
            break;
        }
    }

    // Key not found, we add a new entry if possible
    if (result == -1)
    {
        if (config->count < config->capacity)
        {
            // NOTE: We do a manual copy to avoid possible overflows on input data

            for (int i = 0; (i < RINI_MAX_KEY_SIZE) && (key[i] != '\0'); i++) config->values[config->count].key[i] = key[i];
            for (int i = 0; (i < RINI_MAX_TEXT_SIZE) && (text[i] != '\0'); i++) config->values[config->count].text[i] = text[i];
            for (int i = 0; (i < RINI_MAX_DESC_SIZE) && (desc[i] != '\0'); i++) config->values[config->count].desc[i] = desc[i];

            config->count++;
            result = 0;
        }
    }

    return result;
}

// Set config value description for existing key
// WARNING: Key must exist to add description, if a description exists, it is updated
int rini_set_config_value_description(rini_config *config, const char *key, const char *desc)
{
    int result = 1;

    for (unsigned int i = 0; i < config->count; i++)
    {
        if (strcmp(key, config->values[i].key) == 0)
        {
            memset(config->values[i].desc, 0, RINI_MAX_DESC_SIZE);
            if (desc != NULL) memcpy(config->values[i].desc, desc, strlen(desc));
            result = 0;
            break;
        }
    }

    return result;
}

//----------------------------------------------------------------------------------
// Module internal functions declaration
//----------------------------------------------------------------------------------
// Get string id from a buffer line containing id-value pair
static int rini_read_config_key(const char *buffer, char *key)
{
    int len = 0;
    while ((buffer[len] != '\0') && (buffer[len] != ' ') && (buffer[len] != RINI_VALUE_DELIMITER)) len++;    // Skip keyentifier

    memcpy(key, buffer, len);

    return len;
}

// Get config string-value from a buffer line containing id-value pair
static int rini_read_config_value_text(const char *buffer, char *text, char *desc)
{
    char *buffer_ptr = (char *)buffer;

    // Expected config line structure:
    // [key][spaces?][delimiter?][spaces?]["?][textValue]["?][spaces?][[;][#]description?]
    // We need to skip spaces, check for delimiter (if required), skip spaces, and get text value

    while ((buffer_ptr[0] != '\0') && (buffer_ptr[0] != ' ')) buffer_ptr++;        // Skip keyentifier

    while ((buffer_ptr[0] != '\0') && (buffer_ptr[0] == ' ')) buffer_ptr++;        // Skip line spaces before text value or delimiter

#if defined(RINI_VALUE_DELIMITER)
    if (buffer_ptr[0] == RINI_VALUE_DELIMITER)
    {
        buffer_ptr++;       // Skip delimiter

        while ((buffer_ptr[0] != '\0') && (buffer_ptr[0] == ' ')) buffer_ptr++;    // Skip line spaces before text value
    }
#endif

    // Now buffer_ptr should be pointing to the start of value

    int len = 0;
    while ((buffer_ptr[len] != '\0') &&
        (buffer_ptr[len] != '\r') &&
        (buffer_ptr[len] != '\n')) len++;     // Get text-value and description length (to the end of line)

    // Now we got the length from text-value start to end of line

    int value_len = len;
    int desc_pos = 0;
#if defined(RINI_VALUE_COMMENTS_DELIMITER)
    // Scan text looking for text-value description (if used)
    for (; desc_pos < len; desc_pos++)
    {
        if (buffer_ptr[desc_pos] == RINI_VALUE_COMMENTS_DELIMITER)
        {
            value_len = desc_pos - 1;
            while (buffer_ptr[value_len] == ' ') value_len--;

            value_len++;

            desc_pos++;   // Skip delimiter and following spaces
            while (buffer_ptr[desc_pos] == ' ') desc_pos++;
            break;
        }
    }
#endif

    // Remove starting double quotes from text (if being used)
    if (buffer_ptr[0] == '\"')
    {
        buffer_ptr++; desc_pos--; len--; value_len--;

        // Remove ending double quotes from text (if being used)
        if (buffer_ptr[value_len - 1] == '\"') { value_len--; }
    }

    // Clear text buffers to be updated
    memset(text, 0, RINI_MAX_TEXT_SIZE);
    memset(desc, 0, RINI_MAX_DESC_SIZE);

    // Copy value-text and description to provided pointers
    memcpy(text, buffer_ptr, value_len);
    memcpy(desc, buffer_ptr + desc_pos, ((len - desc_pos) > (RINI_MAX_DESC_SIZE - 1))? (RINI_MAX_DESC_SIZE - 1) : (len - desc_pos));

    return len;
}

// Convert text to int value (if possible), same as atoi()
static int rini_text_to_int(const char *text)
{
    int value = 0;
    int sign = 1;

    if ((text[0] == '+') || (text[0] == '-'))
    {
        if (text[0] == '-') sign = -1;
        text++;
    }

    int i = 0;
    for (; ((text[i] >= '0') && (text[i] <= '9')); i++) value = value*10 + (int)(text[i] - '0');

    //if (strlen(text) != i) // Text starts with numbers but contains some text after that...

    return value*sign;
}

#endif  // RINI_IMPLEMENTATION
