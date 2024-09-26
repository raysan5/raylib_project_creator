/*******************************************************************************************
*
*   raylib projects creator
*
*   FEATURES:
*       - Generate complete build systems: Makefile, VSCode, VS2022
*       - Generate complete GitHub project, ready to upload
-       - Generate preconfigured GitHub Actions, ready to run
*       - WEB: Download generated template as a .zip file
*
*   LIMITATIONS:
*       - Script: build.bat requires Makefile, it could be a cmd/shell script instead
*       - VSCode: Requires compiler tools (make.exe) in the system path
*
*   CONFIGURATION:
*       #define CUSTOM_MODAL_DIALOGS
*           Use custom raygui generated modal dialogs instead of native OS ones
*           NOTE: Avoids including tinyfiledialogs depencency library
*
*   VERSIONS HISTORY:
*       1.0  (26-Sep-2024)  First release
*
*   DEPENDENCIES:
*       raylib 5.5-dev          - Windowing/input management and drawing
*       raygui 4.5-dev          - Immediate-mode GUI controls with custom styling and icons
*       tinyfiledialogs 3.18    - Open/save file dialogs, it requires linkage with comdlg32 and ole32 libs
*       miniz 2.2.0             - Save .zip package file (required for multiple images export)
*
*   COMPILATION (Windows - MinGW):
*       gcc -o $(NAME_PART).exe $(FILE_NAME) -I../../src -lraylib -lopengl32 -lgdi32 -std=c99
*
*
*   LICENSE: zlib/libpng
*
*   Copyright (c) 2024 Ramon Santamaria (@raysan5)
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

#include "raylib.h"

#if defined(PLATFORM_WEB)
    #define CUSTOM_MODAL_DIALOGS            // Force custom modal dialogs usage
    #include <emscripten/emscripten.h>      // Emscripten library - LLVM to JavaScript compiler
#endif

#define RAYGUI_IMPLEMENTATION
#include "external/raygui.h"

#undef RAYGUI_IMPLEMENTATION        // Avoid including raygui implementation again

#define GUI_FILE_DIALOGS_IMPLEMENTATION
#include "gui_file_dialogs.h"       // GUI: File Dialogs

#include "style_amber.h"            // raygui style: amber

// miniz: Single C source file zlib-replacement library
// https://github.com/richgel999/miniz
#include "external/miniz.h"         // ZIP packaging functions definition
#include "external/miniz.c"         // ZIP packaging implementation

// C standard library
#include <stdlib.h>                 // Required for: NULL, calloc(), free()
#include <string.h>                 // Required for: memcpy()

//----------------------------------------------------------------------------------
// Defines and Macros
//----------------------------------------------------------------------------------
#if (!defined(_DEBUG) && (defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)))
bool __stdcall FreeConsole(void);       // Close console from code (kernel32.lib)
#endif

// Simple log system to avoid printf() calls if required
// NOTE: Avoiding those calls, also avoids const strings memory usage
#define SUPPORT_LOG_INFO
#if defined(SUPPORT_LOG_INFO)// && defined(_DEBUG)
    #define LOG(...) printf(__VA_ARGS__)
#else
    #define LOG(...)
#endif

//#define BUILD_TEMPLATE_INTO_EXE

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------

// Project configuration data
// NOTE: Some fields could be not used at the moment
typedef struct ProjectConfig {
    struct {
        int type;                   // Project type to generate: Basic, Advanced, Custom
        char name[64];              // Project name
        char product[64];           // Project product name
        char description[256];      // Project description
        char developer[64];         // Project developer/company
        char developerWeb[64];      // Project developer webpage
        int srcFileCount;           // Project source files count
        char srcFilePaths[64][256]; // Project source files path(s) -> MAX_SOURCE_FILES=64
        char resourcePath[256];     // Project resources directory path
        char includePath[256];      // Project additional include path
        char libPath[256];          // Project additional library path
        char buildPath[256];        // Project build path (for VS2022 defaults to 'build' directory)
    } project;
    struct {
        int flags;                  // Build systems required: Script, Makefile, VSCode, VS2022
        char raylibSrcPath[256];    // raylib source path (validated)
        char compilerPath[256];     // GCC compiler path (w64devkit)
        char emsdkPath[256];        // Emscripten SDK path (WebAssembly)
        char outputPath[256];       // Output path
    } building;
} ProjectConfig;

// Packed file entry
// NOTE: Used for template packing and attach to executable
typedef struct PackFileEntry {
    int fileSize;
    int compFileSize;
    char filePath[256];
} PackFileEntry;

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
#if defined(PLATFORM_DESKTOP)
// Command line functionality
static void ShowCommandLineInfo(void);                      // Show command line usage info
static void ProcessCommandLine(int argc, char *argv[]);     // Process command line input
#endif

static void SetupProject(ProjectConfig *config);            // Setup project, using template

// Packing and unpacking of template files (NOT USED)
static char *PackDirectoryData(const char *baseDirPath, int *packSize);
static void UnpackDirectoryData(const char *outputDirPath, const unsigned char *data, int *dataSize, PackFileEntry *entries, int fileCount);

// Split string into multiple strings
// NOTE: No memory is dynamically allocated
static const char **GetSubtextPtrs(char *text, char delimiter, int *count);

//----------------------------------------------------------------------------------
// Global Variables Definition
//----------------------------------------------------------------------------------
static char inFileName[512] = { 0 };        // Input file name (required in case of drag & drop over executable)
static char outFileName[512] = { 0 };       // Output file name (required for file save/export)

static bool showLoadSourceFilesDialog = false;
static bool showLoadResourcePathDialog = false;
static bool showLoadRaylibSourcePathDialog = false;
static bool showLoadCompilerPathDialog = false;
static bool showLoadOutputPathDialog = false;
static bool showExportProjectProgress = false;
static bool showInfoMessagePanel = false;
static const char *infoTitle = NULL;
static const char *infoMessage = NULL;
static const char *infoButton = NULL;

static bool lockBackground = false;

static char **srcFileNameList = NULL;

static float exportProjectProgress = 0.0f;

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
int main(int argc, char *argv[])
{
#if defined(BUILD_TEMPLATE_INTO_EXE)
    // Attach template data into generated executable on first run
    int exeFileDataSize = 0;
    unsigned char *exeFileData = LoadFileData(argv[0], &exeFileDataSize);
    
    // Check if template already added to not add it again
    char fourcc[5] = { 0 };
    memcpy(fourcc, exeFileData + exeFileDataSize - 4, 4);
    
    if ((fourcc[0] != 'r') || (fourcc[1] != 'p') || (fourcc[2] != 'c') || (fourcc[3] != 'h'))
    {
        // No template data attached to exe, so we attach it
        int packDataSize = 0;
        char *packData = PackDirectoryData(TextFormat("%s/template", GetApplicationDirectory()), &packDataSize);
        
        int outExeFileDataSize = exeFileDataSize + packDataSize;
        char *outExeFileData = (char *)RL_CALLOC(outExeFileDataSize, 1);
        memcpy(outExeFileData, exeFileData, exeFileDataSize);
        memcpy(outExeFileData + exeFileDataSize, packData, packDataSize);
        
        SaveFileData(TextFormat("%s.template.exe", GetFileNameWithoutExt(argv[0])), outExeFileData, outExeFileDataSize);
        
        RL_FREE(outExeFileData);
        RL_FREE(packData);
    }
#endif

#if !defined(_DEBUG)
    SetTraceLogLevel(LOG_NONE);         // Disable raylib trace log messsages
#endif
#if defined(PLATFORM_DESKTOP)
    // Command-line usage mode
    //--------------------------------------------------------------------------------------
    if (argc > 1)
    {
        if ((argc == 2) &&
            (strcmp(argv[1], "-h") != 0) &&
            (strcmp(argv[1], "--help") != 0))       // One argument (file dropped over executable?)
        {
            if (IsFileExtension(argv[1], ".c"))
            {
                ProjectConfig *config = (ProjectConfig *)RL_CALLOC(1, sizeof(ProjectConfig));
                
                config->project.type = 2;  // Custom files
                strcpy(config->project.name, GetFileNameWithoutExt(argv[1]));
                strcpy(config->project.product, GetFileNameWithoutExt(argv[1]));
                strcpy(config->project.description, "My cool project");
                strcpy(config->project.developer, "raylibtech");
                strcpy(config->project.developerWeb, "www.raylibtech.com");
                strcpy(config->project.srcFilePaths[0], argv[1]);
                config->project.srcFileCount = 1;
                strcpy(config->building.compilerPath, "C:\\raylib\\w64devkit\\bin");
                strcpy(config->building.raylibSrcPath, "C:\\raylib\\raylib\\src");
                strcpy(config->building.outputPath, GetDirectoryPath(argv[1]));
                
                SetupProject(config);
                
                RL_FREE(config);
                
                return 0;
            }
        }
        else
        {
            // Get multiple input code files?
            /*
            for (int i = 1; i < argc; i++)
            {
                strcpy(config->project.srcFilePaths[config->project.srcFileCount], argv[i]);
                config->project.srcFileCount++;
            }
            */
            ProcessCommandLine(argc, argv);
            return 0;
        }
    }
#endif  // PLATFORM_DESKTOP
#if (!defined(_DEBUG) && (defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)))
    // WARNING (Windows): If program is compiled as Window application (instead of console),
    // no console is available to show output info... solution is compiling a console application
    // and closing console (FreeConsole()) when changing to GUI interface
    //FreeConsole();
#endif

    // GUI usage mode - Initialization
    //--------------------------------------------------------------------------------------
    // Initialization
    //---------------------------------------------------------------------------------------
    int screenWidth = 800;
    int screenHeight = 536;

    InitWindow(screenWidth, screenHeight, "raylib project creator");
    SetExitKey(0);
    
    // Initialize project config default
    ProjectConfig *config = (ProjectConfig *)RL_CALLOC(1, sizeof(ProjectConfig));
    config->project.type = 2;  // Custom files
    strcpy(config->project.name, "cool_project");
    strcpy(config->project.product, "Cool Project");
    strcpy(config->project.description, "my new cool project");
    strcpy(config->project.developer, "raylib technologies");
    strcpy(config->project.developerWeb, "www.raylibtech.com");
    //strcpy(config->project.srcFilePaths[0], argv[1]);
    //config->project.srcFileCount = 1;
    strcpy(config->building.compilerPath, "C:\\raylib\\w64devkit\\bin");
    strcpy(config->building.raylibSrcPath, "C:\\raylib\\raylib\\src");
    strcpy(config->building.outputPath, ".");
    
    // Source file names (without path) are used for display on source textbox
    srcFileNameList = (char **)RL_CALLOC(256, sizeof(char *)); // Max number of input source files supported
    for (int i = 0; i < 256; i++) srcFileNameList[i] = (char *)RL_CALLOC(256, sizeof(char));
    
    // GUI: Main Layout
    //-----------------------------------------------------------------------------------
    Vector2 anchorProject = { 8, 64 };
    Vector2 anchorBuilding = { 8, 258 };
    
    bool projectNameEditMode = false;
    strcpy(config->project.name, "cool_project");
    bool productNameEditMode = false;
    strcpy(config->project.product, "Cool Project");
    bool projectDeveloperEditMode = false;
    strcpy(config->project.developer, "raylibtech");
    bool projectDeveloperWebEditMode = false;
    strcpy(config->project.developerWeb, "www.raylibtech.com"); 
    bool projectDescriptionEditMode = false;
    strcpy(config->project.description, "my cool new project");
    bool projectSourceFilePathEditMode = false;
    bool projectResourcePathEditMode = false;
    bool buildingRaylibPathEditMode = false;
    bool buildingCompilerPathEditMode = false;
    bool buildingOutputPathEditMode = false;
    
    GuiLoadStyleAmber();    // Load UI style
 
    GuiEnableTooltip();     // Enable tooltips by default
    //----------------------------------------------------------------------------------
    
    // GUI: Exit Window
    //-----------------------------------------------------------------------------------
    bool closeWindow = false;
    bool showExitWindow = false;
    //-----------------------------------------------------------------------------------

    infoTitle = "WELCOME! LET'S CREATE A PROJECT!";
    infoMessage = "Provide some source code files (.c) to generate project!";// \nOr choose a default project type!";
    infoButton = "Sure! Let's start!";
    showInfoMessagePanel = true;
    
    LOG("INIT: Ready to show project generation info...\n");

    SetTargetFPS(60);
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!closeWindow)    // Detect window close button
    {
        // WARNING: ASINCIFY requires this line,
        // it contains the call to emscripten_sleep() for PLATFORM_WEB
        if (WindowShouldClose()) closeWindow = true;

        // Dropped files logic
        //----------------------------------------------------------------------------------
        if (IsFileDropped())
        {
            FilePathList droppedFiles = LoadDroppedFiles();
            
            for (int i = 0; i < droppedFiles.count; i++)
            {
                if (IsFileExtension(droppedFiles.paths[i], ".c;.h"))
                {
                    // Add files to source list
                    strcpy(config->project.srcFilePaths[config->project.srcFileCount], droppedFiles.paths[i]);
                    strcpy(srcFileNameList[config->project.srcFileCount], GetFileName(droppedFiles.paths[i]));
                    config->project.srcFileCount++;
                }
            }
            
            if (IsFileExtension(droppedFiles.paths[0], ".rgs")) 
            {
                // Reset to default internal style
                // NOTE: Required to unload any previously loaded font texture
                GuiLoadStyleDefault();
                GuiLoadStyle(droppedFiles.paths[0]);
            }

            UnloadDroppedFiles(droppedFiles);    // Unload filepaths from memory
        }
        //----------------------------------------------------------------------------------
        
        // Keyboard shortcuts
        //------------------------------------------------------------------------------------
        // Show dialog: export project
        if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_S))
        {
            strcpy(outFileName, TextToLower("config->project.name"));
            showExportProjectProgress = true;
        }

        // Show dialog: load source files
        if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_O)) showLoadSourceFilesDialog = true;

        // Show closing window on ESC
        if (IsKeyPressed(KEY_ESCAPE))
        {
            if (0) { }
        #if defined(PLATFORM_DESKTOP)
            else if (showInfoMessagePanel) showInfoMessagePanel = false;
            else showExitWindow = !showExitWindow;
        #else
            else if (showLoadSourceFilesDialog) showLoadSourceFilesDialog = false;
            else if (showLoadResourcePathDialog) showLoadResourcePathDialog = false;
            else if (showLoadRaylibSourcePathDialog) showLoadRaylibSourcePathDialog = false;
            else if (showLoadCompilerPathDialog) showLoadCompilerPathDialog = false;
            else if (showLoadOutputPathDialog) showLoadCompilerPathDialog = false;
            else if (showExportProjectProgress) showExportProjectProgress = false;
        #endif
        }
        //----------------------------------------------------------------------------------
        
        // Basic program flow logic
        //----------------------------------------------------------------------------------
        if (showExitWindow ||
            showInfoMessagePanel ||
            showLoadSourceFilesDialog ||
            showLoadResourcePathDialog ||
            showLoadRaylibSourcePathDialog ||
            showLoadCompilerPathDialog ||
            showLoadOutputPathDialog ||
            showExportProjectProgress) lockBackground = true;
        else lockBackground = false;
                
        if (lockBackground) GuiLock();
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();
            ClearBackground(GetColor(GuiGetStyle(DEFAULT, BACKGROUND_COLOR))); 

            // GUI: Main Window
            //----------------------------------------------------------------------------------
            int prevProjectType = config->project.type;
            GuiLabel((Rectangle){ 16, 12, 104, 32 }, "PROJECT TYPE:");
            //GuiSetTooltip("Choose from a default project template or custom source files");
            GuiToggleGroup((Rectangle){ 120, 12, 223, 32 }, "Basic Sample;Screen Manager;Custom files", &config->project.type);
            GuiSetTooltip(NULL);
            
            if (config->project.type != prevProjectType)
            {
                if (config->project.type == 0)
                {
                    strcpy(srcFileNameList[0], TextFormat("%s.c", TextToLower(config->project.name)));
                    config->project.srcFileCount = 1;
                }
                else if (config->project.type == 1)
                {
                    strcpy(srcFileNameList[0], TextFormat("%s.c", TextToLower(config->project.name)));
                    strcpy(srcFileNameList[1], "screens.h");
                    strcpy(srcFileNameList[2], "screen_logo.c");
                    strcpy(srcFileNameList[3], "screen_title.c");
                    strcpy(srcFileNameList[4], "screen_options.c");
                    strcpy(srcFileNameList[5], "screen_gameplay.c");
                    strcpy(srcFileNameList[6], "screen_ending.c");
                    config->project.srcFileCount = 7;
                }
                else if (config->project.type == 2)
                {
                    config->project.srcFileCount = 0;
                }
            }
            
            GuiGroupBox((Rectangle){ anchorProject.x + 0, anchorProject.y + 0, 784, 172 }, "PROJECT SETTINGS");
            GuiLabel((Rectangle){ anchorProject.x + 8, anchorProject.y + 24, 104, 24 }, "PROJECT NAME:");
            //GuiSetTooltip("Define project name, note that every project\nblablablaballsadlksad");
            if (GuiTextBox((Rectangle){ anchorProject.x + 112, anchorProject.y + 24, 280, 24 }, config->project.name, 128, projectNameEditMode)) projectNameEditMode = !projectNameEditMode;
            GuiLabel((Rectangle){ anchorProject.x + 408, anchorProject.y + 24, 80, 24 }, "PRODUCT NAME:");
            if (GuiTextBox((Rectangle){ anchorProject.x + 496, anchorProject.y + 24, 280, 24 }, config->project.product, 128, productNameEditMode)) productNameEditMode = !productNameEditMode;
            GuiLabel((Rectangle){ anchorProject.x + 8, anchorProject.y + 56, 104, 24 }, "DESCRIPTION:");
            if (GuiTextBox((Rectangle){ anchorProject.x + 112, anchorProject.y + 56, 664, 24 }, config->project.description, 128, projectDescriptionEditMode)) projectDescriptionEditMode = !projectDescriptionEditMode;
            GuiLabel((Rectangle){ anchorProject.x + 8, anchorProject.y + 88, 104, 24 }, "DEVELOPER:");
            if (GuiTextBox((Rectangle){ anchorProject.x + 112, anchorProject.y + 88, 280, 24 }, config->project.developer, 128, projectDeveloperEditMode)) projectDeveloperEditMode = !projectDeveloperEditMode;
            GuiLabel((Rectangle){ anchorProject.x + 408, anchorProject.y + 88, 80, 24 }, "DEV. WEBPAGE:");
            if (GuiTextBox((Rectangle){ anchorProject.x + 496, anchorProject.y + 88, 280, 24 }, config->project.developerWeb, 128, projectDeveloperWebEditMode)) projectDeveloperWebEditMode = !projectDeveloperWebEditMode;
            
            if (config->project.type != 2) GuiDisable();
            GuiLabel((Rectangle){ anchorProject.x + 8, anchorProject.y + 128, 104, 24 }, "SOURCE FILES:");
            GuiSetStyle(TEXTBOX, TEXT_READONLY, 1);
            GuiTextBox((Rectangle){ anchorProject.x + 112, anchorProject.y + 128, 536, 24 }, TextJoin(srcFileNameList, config->project.srcFileCount, ";"), 256, projectSourceFilePathEditMode);//) projectSourceFilePathEditMode = !projectSourceFilePathEditMode;
            GuiSetStyle(TEXTBOX, TEXT_READONLY, 0);
            if (GuiButton((Rectangle){ anchorProject.x + 656, anchorProject.y + 128, 120, 24 }, "Browse")) showLoadSourceFilesDialog = true; 
            //GuiLabel((Rectangle){ anchorProject.x + 8, anchorProject.y + 160, 104, 24 }, "RESOURCE PATH:");
            //if (GuiTextBox((Rectangle){ anchorProject.x + 112, anchorProject.y + 160, 536, 24 }, config->project.resourcePath, 128, projectResourcePathEditMode)) projectResourcePathEditMode = !projectResourcePathEditMode;
#if defined(PLATFORM_WEB)
            //GuiDisable();
#endif
            //if (GuiButton((Rectangle){ anchorProject.x + 656, anchorProject.y + 160, 120, 24 }, "Browse")) showLoadResourcePathDialog = true;
            GuiEnable();
            
            GuiGroupBox((Rectangle){ anchorBuilding.x + 0, anchorBuilding.y + 0, 784, 136 }, "BUILD SETTINGS");
            GuiLabel((Rectangle){ anchorBuilding.x + 8, anchorBuilding.y + 16, 104, 24 }, "raylib SRC PATH:");
            if (GuiTextBox((Rectangle){ anchorBuilding.x + 112, anchorBuilding.y + 16, 536, 24 }, config->building.raylibSrcPath, 128, buildingRaylibPathEditMode)) buildingRaylibPathEditMode = !buildingRaylibPathEditMode;
#if defined(PLATFORM_WEB)
            GuiDisable();
#endif
            if (GuiButton((Rectangle){ anchorBuilding.x + 656, anchorBuilding.y + 16, 120, 24 }, "Browse")) showLoadRaylibSourcePathDialog = true;
            GuiEnable();
            GuiLabel((Rectangle){ anchorBuilding.x + 8, anchorBuilding.y + 48, 104, 24 }, "COMPILER PATH:");
            if (GuiTextBox((Rectangle){ anchorBuilding.x + 112, anchorBuilding.y + 48, 536, 24 }, config->building.compilerPath, 128, buildingCompilerPathEditMode)) buildingCompilerPathEditMode = !buildingCompilerPathEditMode;
#if defined(PLATFORM_WEB)
            GuiDisable();
#endif
            if (GuiButton((Rectangle){ anchorBuilding.x + 656, anchorBuilding.y + 48, 120, 24 }, "Browse")) showLoadCompilerPathDialog = true;
            GuiEnable();
            GuiLabel((Rectangle){ anchorBuilding.x + 8, anchorBuilding.y + 88, 104, 32 }, "BUILD SYSTEMS:");
            
            if (!lockBackground) GuiLock();
            bool buildSystem = true;
            GuiToggle((Rectangle){ anchorBuilding.x + 112, anchorBuilding.y + 88, 164, 32 }, "Script", &buildSystem);
            GuiToggle((Rectangle){ anchorBuilding.x + 112 + 166, anchorBuilding.y + 88, 164, 32 }, "Makefile", &buildSystem);
            GuiToggle((Rectangle){ anchorBuilding.x + 112 + 166*2, anchorBuilding.y + 88, 164, 32 }, "VSCode", &buildSystem);
            GuiToggle((Rectangle){ anchorBuilding.x + 112 + 166*3, anchorBuilding.y + 88, 164, 32 }, "VS2022", &buildSystem);
            if (!lockBackground) GuiUnlock();
            
#if defined(PLATFORM_WEB)
            GuiDisable();
#endif
            GuiLabel((Rectangle){ anchorBuilding.x + 8, anchorBuilding.y + 152, 104, 24 }, "OUTPUT PATH:");
            if (GuiTextBox((Rectangle){ anchorBuilding.x + 112, anchorBuilding.y + 152, 536, 24 }, config->building.outputPath, 128, buildingOutputPathEditMode)) buildingOutputPathEditMode = !buildingOutputPathEditMode;
            if (GuiButton((Rectangle){ anchorBuilding.x + 656, anchorBuilding.y + 152, 120, 24 }, "Browse")) showLoadOutputPathDialog = true;
            GuiEnable();
            
            if (config->project.srcFileCount == 0) GuiDisable();
            if (GuiButton((Rectangle){ 8, 450, 784, 40 }, "GENERATE PROJECT STRUCTURE"))
            {
                SetupProject(config);
                showExportProjectProgress = true;
            }
            GuiEnable();

            if (!lockBackground && CheckCollisionPointRec(GetMousePosition(), (Rectangle){ 0, GetScreenHeight() - 32, GetScreenWidth(), 32 })) SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
            else SetMouseCursor(MOUSE_CURSOR_DEFAULT);

            if (GuiButton((Rectangle){ 0, GetScreenHeight() - 32, GetScreenWidth(), 32 },
                "Did you find this tool useful? Please, consider a donation! Thanks! <3"))
            {
                OpenURL("https://github.com/sponsors/raysan5");
            }
            //----------------------------------------------------------------------------------
            
            // GUI: Status bar
            //----------------------------------------------------------------------------------
            //int textPadding = GuiGetStyle(STATUSBAR, TEXT_PADDING);
            //GuiSetStyle(STATUSBAR, TEXT_PADDING, 0);
            //GuiSetStyle(STATUSBAR, TEXT_ALIGNMENT, TEXT_ALIGN_CENTER);
            //GuiStatusBar((Rectangle){ 0, screenHeight - 24, GetScreenWidth(), 24 }, "PROJECT INFO");
            //GuiSetStyle(STATUSBAR, TEXT_ALIGNMENT, TEXT_ALIGN_LEFT);
            //GuiSetStyle(STATUSBAR, TEXT_PADDING, textPadding);
            //----------------------------------------------------------------------------------

            // NOTE: If some overlap window is open and main window is locked, we draw a background rectangle
            if (lockBackground) DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(GetColor(GuiGetStyle(DEFAULT, BACKGROUND_COLOR)), 0.85f));

            // WARNING: Before drawing the windows, we unlock them
            GuiUnlock();
            
            // GUI: Show info message panel
            //----------------------------------------------------------------------------------------
            if (showInfoMessagePanel)
            {
                Vector2 textSize = MeasureTextEx(GuiGetFont(), infoMessage, GuiGetFont().baseSize*2, 3);
                GuiPanel((Rectangle){ -10, GetScreenHeight()/2 - 180, GetScreenWidth() + 20, 290 }, NULL);

                GuiSetStyle(DEFAULT, TEXT_SIZE, GuiGetFont().baseSize*3);
                GuiSetStyle(DEFAULT, TEXT_SPACING, 3);
                GuiSetStyle(LABEL, TEXT_ALIGNMENT, TEXT_ALIGN_CENTER);
                GuiSetStyle(LABEL, TEXT_COLOR_NORMAL, GuiGetStyle(DEFAULT, TEXT_COLOR_FOCUSED));
                GuiLabel((Rectangle){ -10, GetScreenHeight()/2 - 140, GetScreenWidth() + 20, 30 }, infoTitle);
                GuiSetStyle(LABEL, TEXT_COLOR_NORMAL, GuiGetStyle(DEFAULT, TEXT_COLOR_NORMAL));
                GuiSetStyle(DEFAULT, TEXT_SIZE, GuiGetFont().baseSize*2);
                GuiLabel((Rectangle){ -10, GetScreenHeight()/2 - textSize.y - 30, GetScreenWidth() + 20, 30 }, infoMessage);

                if (GuiButton((Rectangle){ GetScreenWidth()/4, GetScreenHeight()/2 + 40, GetScreenWidth()/2, 40 }, infoButton)) 
                {
                    showInfoMessagePanel = false;
                    
                    infoTitle = "WARNING! READ CAREFULLY!";
                    infoMessage = NULL;
                    infoButton = "I understand implications";
                }

                GuiSetStyle(LABEL, TEXT_ALIGNMENT, TEXT_ALIGN_LEFT);
                GuiSetStyle(DEFAULT, TEXT_SIZE, GuiGetFont().baseSize);
                GuiSetStyle(DEFAULT, TEXT_SPACING, 1);
            }
            //----------------------------------------------------------------------------------------
            
            // GUI: Exit Window
            //----------------------------------------------------------------------------------------
            if (showExitWindow)
            {
                int result = GuiMessageBox((Rectangle){ (float)screenWidth/2 - 125, (float)screenHeight/2 - 50, 250, 100 }, "#159#Closing raylib project creator", "Do you really want to exit?", "Yes;No");

                if ((result == 0) || (result == 2)) showExitWindow = false;
                else if (result == 1) closeWindow = true;
            }
            //----------------------------------------------------------------------------------------

            // GUI: Load Source Files Dialog
            //----------------------------------------------------------------------------------------
            if (showLoadSourceFilesDialog)
            {
                // Multiple files selected are supported, tinyfiledialogs returns them as: path01|path02|path03|path04
                // We must reserve enough memory to deal with the maximum amount of data that tinyfiledialogs can provide
                // tinifiledialogs limits defined:
                //#define MAX_MULTIPLE_FILES 1024
                //#define MAX_PATH_OR_CMD 1024
#if defined(PLATFORM_WEB)
                char *multiFileNames = NULL;
#else
                char *multiFileNames = (char *)RL_CALLOC(1024*1024, 1); // Let's reserve for 1024 paths, 1024 bytes each
#endif
#if defined(CUSTOM_MODAL_DIALOGS)
                int result = GuiFileDialog(DIALOG_MESSAGE, "Load source file(s)...", inFileName, "Ok", "Just drag and drop your code file(s)!");
#else
                int result = GuiFileDialog(DIALOG_OPEN_FILE_MULTI, "Load source file(s)...", multiFileNames, "*.c,*.h", "Code Files (*.c,*.h)");
#endif
                if (result == 1)
                {
                    int multiFileCount = 0;
                    const char **multiFileList = GetSubtextPtrs(multiFileNames, '|', &multiFileCount); // Split text into multiple strings

                    for (int i = 0; i < multiFileCount; i++)
                    {
                        if (IsFileExtension(multiFileList[i], ".c;.h") && (config->project.srcFileCount < 256))
                        {
                            // Add files to source list
                            strcpy(config->project.srcFilePaths[config->project.srcFileCount], multiFileList[i]);
                            strcpy(srcFileNameList[config->project.srcFileCount], GetFileName(multiFileList[i]));
                            config->project.srcFileCount++;
                            
                            if (config->project.srcFileCount >= 256) break;
                        }
                    }
                }

                if (result >= 0) showLoadSourceFilesDialog = false;
            }
            //----------------------------------------------------------------------------------------
            
            // GUI: Load Resource Path Dialog
            //----------------------------------------------------------------------------------------
            if (showLoadResourcePathDialog)
            {
#if defined(CUSTOM_MODAL_DIALOGS)
                int result = GuiFileDialog(DIALOG_MESSAGE, "Select resource directory...", inFileName, "Ok", "Edit the path in text box");
#else
                int result = GuiFileDialog(DIALOG_OPEN_DIRECTORY, "Select resources directory...", inFileName, NULL, NULL);
#endif
                if (result == 1)
                {
                    if (DirectoryExists(inFileName)) strcpy(config->project.resourcePath, inFileName);
                    else
                    {
                        infoMessage = "Provided resource path does not exist!";
                        showInfoMessagePanel = true;
                    }
                }

                if (result >= 0) showLoadResourcePathDialog = false;
            }
            //----------------------------------------------------------------------------------------
            
            // GUI: Load raylib Source Path Dialog
            //----------------------------------------------------------------------------------------
            if (showLoadRaylibSourcePathDialog)
            {
#if defined(CUSTOM_MODAL_DIALOGS)
                int result = GuiFileDialog(DIALOG_MESSAGE, "Select raylib src directory...", inFileName, "Ok", "Edit the path in text box");
#else
                int result = GuiFileDialog(DIALOG_OPEN_DIRECTORY, "Select raylib src directory...", inFileName, NULL, NULL);
#endif
                if (result == 1)
                {
                    // Check raylib path
                    if (FileExists(TextFormat("%s/raylib.h", inFileName))) strcpy(config->building.raylibSrcPath, inFileName);
                    else
                    {
                        infoMessage = "Provided raylib source path does not included raylib.h!";
                        showInfoMessagePanel = true;
                        strcpy(config->building.raylibSrcPath, inFileName);
                    }
                }

                if (result >= 0) showLoadRaylibSourcePathDialog = false;
            }
            //----------------------------------------------------------------------------------------
            
            // GUI: Load Compiler Path Dialog
            //----------------------------------------------------------------------------------------
            if (showLoadCompilerPathDialog)
            {
#if defined(CUSTOM_MODAL_DIALOGS)
                int result = GuiFileDialog(DIALOG_MESSAGE, "Select GCC compiler directory...", inFileName, "Ok", "Edit the path in text box");
#else
                int result = GuiFileDialog(DIALOG_OPEN_DIRECTORY, "Select GCC compiler directory...", inFileName, NULL, NULL);
#endif
                if (result == 1)
                {
                    // Check compiler path
                    if (FileExists(TextFormat("%s/gcc.exe", inFileName))) strcpy(config->building.compilerPath, inFileName);
                    else
                    {
                        infoMessage = "Provided compiler path does not included gcc.exe!";
                        showInfoMessagePanel = true;
                        strcpy(config->building.compilerPath, inFileName);
                    }
                }

                if (result >= 0) showLoadCompilerPathDialog = false;
            }
            //----------------------------------------------------------------------------------------
            
            // GUI: Load Output Path Dialog
            //----------------------------------------------------------------------------------------
            if (showLoadOutputPathDialog)
            {
#if defined(CUSTOM_MODAL_DIALOGS)
                int result = GuiFileDialog(DIALOG_MESSAGE, "Select generation output directory...", inFileName, "Ok", "Edit the path in text box");
#else
                int result = GuiFileDialog(DIALOG_OPEN_DIRECTORY, "Select generation output directory...", inFileName, NULL, NULL);
#endif
                if (result == 1)
                {
                    strcpy(config->building.outputPath, inFileName);
                }

                if (result >= 0) showLoadOutputPathDialog = false;
            }
            //----------------------------------------------------------------------------------------

            // GUI: Export Project Dialog (and saving logic)
            //----------------------------------------------------------------------------------------
            if (showExportProjectProgress)
            {
                GuiPanel((Rectangle){ -10, GetScreenHeight()/2 - 100, GetScreenWidth() + 20, 200 }, NULL);

                GuiSetStyle(DEFAULT, TEXT_SIZE, GuiGetFont().baseSize*3);
                GuiSetStyle(DEFAULT, TEXT_SPACING, 3);
                GuiSetStyle(LABEL, TEXT_ALIGNMENT, TEXT_ALIGN_CENTER);
                GuiSetStyle(LABEL, TEXT_COLOR_NORMAL, GuiGetStyle(DEFAULT, TEXT_COLOR_FOCUSED));
                GuiLabel((Rectangle){ -10, GetScreenHeight()/2 - 60, GetScreenWidth() + 20, 30 }, ((int)exportProjectProgress >= 100)? "PROJECT GENERATED SUCCESSFULLY" : "GENERATING PROJECT...");
                GuiSetStyle(LABEL, TEXT_COLOR_NORMAL, GuiGetStyle(DEFAULT, TEXT_COLOR_NORMAL));
                GuiSetStyle(DEFAULT, TEXT_SIZE, GuiGetFont().baseSize*2);
                
                exportProjectProgress += 2.0f;
                GuiProgressBar((Rectangle){ 12, GetScreenHeight()/2, GetScreenWidth() - 24, 20 }, NULL, NULL, &exportProjectProgress, 0, 100);

                if (exportProjectProgress < 100.0f) GuiDisable();
                if (GuiButton((Rectangle){ GetScreenWidth()/4, GetScreenHeight()/2 + 40, GetScreenWidth()/2, 40 }, "GREAT!")) 
                {
                    showExportProjectProgress = false;
                }
                GuiEnable();

                GuiSetStyle(LABEL, TEXT_ALIGNMENT, TEXT_ALIGN_LEFT);
                GuiSetStyle(DEFAULT, TEXT_SIZE, GuiGetFont().baseSize);
                GuiSetStyle(DEFAULT, TEXT_SPACING, 1);

                if (!showExportProjectProgress)
                {
                #if defined(PLATFORM_WEB)
                    strcpy(outFileName, TextFormat("%s/%s", config->building.outputPath, TextToLower(config->project.name)));

                    // Package all created files (in browser MEMFS) into a .zip to be exported
                    mz_zip_archive zip = { 0 };
                    mz_bool mz_ret = mz_zip_writer_init_file(&zip, TextFormat("%s.zip", outFileName), 0);
                    if (!mz_ret) LOG("WARNING: Could not initialize zip archive\n");
                    
                    FilePathList files = LoadDirectoryFilesEx(outFileName, NULL, true);
                    
                    // Add all template updated files to zip
                    for (int i = 0; i < files.count; i++)
                    {
                        // WARNING: We need to move the directory path a bit to skip "././" and "./"
                        mz_ret = mz_zip_writer_add_file(&zip,
                            TextFormat("%s/%s", GetDirectoryPath(files.paths[i]) + 4, GetFileName(files.paths[i])),
                            TextFormat("%s/%s", GetDirectoryPath(files.paths[i]) + 2, GetFileName(files.paths[i])),
                            "Comment", (mz_uint16)strlen("Comment"), MZ_BEST_SPEED);
                        if (!mz_ret) printf("Could not add file to zip archive\n");
                    }

                    mz_ret = mz_zip_writer_finalize_archive(&zip);
                    if (!mz_ret) LOG("WARNING: Could not finalize zip archive\n");

                    mz_ret = mz_zip_writer_end(&zip);
                    if (!mz_ret) LOG("WARNING: Could not finalize zip writer\n");
                    
                    UnloadDirectoryFiles(files);

                    char tempFileName[512] = { 0 };
                    strcpy(tempFileName, TextFormat("%s.zip", outFileName));
                    emscripten_run_script(TextFormat("saveFileFromMEMFSToDisk('%s','%s')", tempFileName, GetFileName(tempFileName)));

                    // Download file from MEMFS (emscripten memory filesystem)
                    // NOTE: Second argument must be a simple filename (we can't use directories)
                    // NOTE: Included security check to (partially) avoid malicious code on PLATFORM_WEB
                    //if (strchr(outFileName, '\'') == NULL) emscripten_run_script(TextFormat("saveFileFromMEMFSToDisk('%s','%s')", outFileName, GetFileName(outFileName)));
                #endif
                }
            }
            //----------------------------------------------------------------------------------------

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    RL_FREE(config);
    
    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}

//--------------------------------------------------------------------------------------------
// Module functions definition
//--------------------------------------------------------------------------------------------
#if defined(PLATFORM_DESKTOP)
// Show command line usage info
static void ShowCommandLineInfo(void)
{
    printf("\n//////////////////////////////////////////////////////////////////////////////////\n");
    printf("//                                                                              //\n");
    printf("// raylib project creator v1.0                                                  //\n");
    printf("// powered by raylib v%s and raygui v%s                               //\n", RAYLIB_VERSION, RAYGUI_VERSION);
    printf("//                                                                              //\n");
    printf("// Copyright (c) 2024 Ramon Santamaria (@raysan5)                               //\n");
    printf("//                                                                              //\n");
    printf("//////////////////////////////////////////////////////////////////////////////////\n\n");

    printf("USAGE:\n\n");
    printf("    > raylib_project_creator [--help] --name <project_name> --src <source_file01.c>,<source_file02.c>\n");
    printf("             [--product <product_name>] [--desc <project_description>]\n");
    printf("             [--dev <developer_name>] [--devweb <developer_webpage>]\n");
    printf("             [--raylib <raylib_src_path>] [--comp <compiler_path>]\n");
    printf("             [--out <output_path>]\n");

    printf("\nOPTIONS:\n\n");
    printf("    -h, --help                      : Show tool version and command line usage help\n\n");
    printf("    -n, --name                      : Define project name\n");
    printf("    -i, --src <source_file01.c>,<source_file02.c>\n");
    printf("                                    : Define input source files(s), comma separated.\n");
    printf("    -p, --product <product_name>    : Define product name, commercial name\n");
    printf("    --desc <project_description>    : Define product description, use \"desc\"\n");
    printf("    --dev <developer_name>          : Define developer name\n");
    printf("    --devweb <developer_webpage>    : Define developer webpage\n");
    printf("    --raylib <raylib_src_path>      : Define raylib src path (raylib.h)\n");
    printf("    --comp <compiler_path>          : Define compiler path (ggc.exe)\n");
    printf("    -o, --out <output_path>         : Define output path for generated project.\n");

    printf("\nEXAMPLES:\n\n");
    printf("    > raylib_project_creator -n rfxgen -p rFXGen --src rfxgen.c\n");
    printf("        Generate <rfxgen> project build system\n");
}

// Process command line input
static void ProcessCommandLine(int argc, char *argv[])
{
    // CLI required variables
    bool showUsageInfo = false;         // Toggle command line usage info

    if (argc == 1) showUsageInfo = true;
    
    ProjectConfig *config = (ProjectConfig *)RL_CALLOC(1, sizeof(ProjectConfig));

    // Process command line arguments
    for (int i = 1; i < argc; i++)
    {
        if ((strcmp(argv[i], "-h") == 0) || (strcmp(argv[i], "--help") == 0))
        {
            showUsageInfo = true;
        }
        else if ((strcmp(argv[i], "-n") == 0) || (strcmp(argv[i], "--name") == 0))
        {
            if (((i + 1) < argc) && (argv[i + 1][0] != '-'))
            {
                strcpy(config->project.name, argv[i + 1]);
            }
            else LOG("WARNING: Name parameter provided not valid\n");
        }
        else if ((strcmp(argv[i], "-i") == 0) || (strcmp(argv[i], "--src") == 0))
        {
            // Check for valid argument and valid file extension
            if (((i + 1) < argc) && (argv[i + 1][0] != '-'))
            {
                // Split provided arg by ',' to get multiple source input files
                int fileCount = 0;
                const char **files = TextSplit(argv[i + 1], ',', &fileCount);

                for (int j = 0; j < fileCount; j++)
                {
                    if (IsFileExtension(files[i], ".h;.c"))
                    {
                        strcpy(config->project.srcFilePaths[config->project.srcFileCount], files[i]);
                        config->project.srcFileCount++;
                    }
                    else LOG("WARNING: [%s] File not recognized as source file (Use: .c,.h)\n", files[i]);
                }
            }
            else LOG("WARNING: No input file provided\n");
        }
        else if ((strcmp(argv[i], "-p") == 0) || (strcmp(argv[i], "--product") == 0))
        {
            if (((i + 1) < argc) && (argv[i + 1][0] != '-'))
            {
                strcpy(config->project.product, argv[i + 1]);
            }
            else LOG("WARNING: Product name parameters provided not valid\n");
        }
        else if (strcmp(argv[i], "--desc") == 0)
        {
            if (((i + 1) < argc) && (argv[i + 1][0] != '-'))
            {
                strcpy(config->project.description, argv[i + 1]);
            }
            else LOG("WARNING: Description parameters provided not valid\n");
        }
        else if (strcmp(argv[i], "--dev") == 0)
        {
            if (((i + 1) < argc) && (argv[i + 1][0] != '-'))
            {
                strcpy(config->project.developer, argv[i + 1]);
            }
            else LOG("WARNING: Developer parameters provided not valid\n");
        }
        else if (strcmp(argv[i], "--devweb") == 0)
        {
            if (((i + 1) < argc) && (argv[i + 1][0] != '-'))
            {
                strcpy(config->project.developerWeb, argv[i + 1]);
            }
            else LOG("WARNING: Developer web parameters provided not valid\n");
        }
        else if (strcmp(argv[i], "--raylib") == 0)
        {
            if (((i + 1) < argc) && (argv[i + 1][0] != '-'))
            {
                strcpy(config->building.raylibSrcPath, argv[i + 1]);
            }
            else LOG("WARNING: raylib source path parameters provided not valid\n");
        }
        else if (strcmp(argv[i], "--comp") == 0)
        {
            if (((i + 1) < argc) && (argv[i + 1][0] != '-'))
            {
                strcpy(config->building.compilerPath, argv[i + 1]);
            }
            else LOG("WARNING: Compiler path parameters provided not valid\n");
        }
        else if ((strcmp(argv[i], "-o") == 0) || (strcmp(argv[i], "--out") == 0))
        {
            if (((i + 1) < argc) && (argv[i + 1][0] != '-'))
            {
                strcpy(config->building.outputPath, argv[i + 1]);
            }
            else LOG("WARNING: Output path provided not valid\n");
        }
    }

    // Generate build projects
    SetupProject(config);
    
    RL_FREE(config);

    if (showUsageInfo) ShowCommandLineInfo();
}
#endif      // PLATFORM_DESKTOP

//--------------------------------------------------------------------------------------------
// Auxiliar functions
//--------------------------------------------------------------------------------------------

// Generate tool project files
// Project input files required to update:
//  - src/project_name.c
//  - src/project_name.rc
//  - src/project_name.ico
//  - src/project_name.icns
//  - src/Info.plist
//  - src/minshell.html
//  - src/Makefile
//  - projects/scripts/*
//  - projects/VS2022/*
//  - projects/VSCode/*
//  - README.md
//  - LICENSE
static void SetupProject(ProjectConfig *config)
{
    char *fileText = NULL;
    char *fileTextUpdated[6] = { 0 };
    
    // Get template directory
    // TODO: Use embedded template into executable?
    char templatePath[256] = { 0 };
    strcpy(templatePath, TextFormat("%s/template", GetApplicationDirectory()));

    // Remove spaces from directories/files names
    char outProjectName[256] = { 0 };
    TextCopy(outProjectName, TextToLower(config->project.name));//TextRemoveSpaces(TextToLower(config->project.name)));

    LOG("INFO: Output path: %s\n", TextFormat("%s/%s", config->building.outputPath, outProjectName));

    // Copy project source file(s) provided
    //--------------------------------------------------------------------------
    // Create required output directories
    MakeDirectory(TextFormat("%s/%s/src/external", config->building.outputPath, outProjectName));
    if (config->project.type == 0)  // Use base sample (one source file)
    {
        fileText = LoadFileText(TextFormat("%s/src/project_name.c", templatePath));
        SaveFileText(TextFormat("%s/%s/src/%s.c", config->building.outputPath, outProjectName, TextToLower(config->project.name)), fileText);
        UnloadFileText(fileText);
        LOG("INFO: Copied src/%s.c successfully\n", TextToLower(config->project.name));
    }
    else if (config->project.type == 1) // Use advance sample (screen manager, multiple source files)
    {
        fileText = LoadFileText(TextFormat("%s/src/raylib_advanced.c", templatePath));
        SaveFileText(TextFormat("%s/%s/src/%s.c", config->building.outputPath, outProjectName, TextToLower(config->project.name)), fileText);
        UnloadFileText(fileText);

        fileText = LoadFileText(TextFormat("%s/src/screens.h", templatePath));
        SaveFileText(TextFormat("%s/%s/src/screens.h", config->building.outputPath, outProjectName), fileText);
        UnloadFileText(fileText);
        fileText = LoadFileText(TextFormat("%s/src/screen_logo.c", templatePath));
        SaveFileText(TextFormat("%s/%s/src/screen_logo.c", config->building.outputPath, outProjectName), fileText);
        UnloadFileText(fileText);
        fileText = LoadFileText(TextFormat("%s/src/screen_title.c", templatePath));
        SaveFileText(TextFormat("%s/%s/src/screen_title.c", config->building.outputPath, outProjectName), fileText);
        UnloadFileText(fileText);
        fileText = LoadFileText(TextFormat("%s/src/screen_options.c", templatePath));
        SaveFileText(TextFormat("%s/%s/src/screen_options.c", config->building.outputPath, outProjectName), fileText);
        UnloadFileText(fileText);
        fileText = LoadFileText(TextFormat("%s/src/screen_gameplay.c", templatePath));
        SaveFileText(TextFormat("%s/%s/src/screen_gameplay.c", config->building.outputPath, outProjectName), fileText);
        UnloadFileText(fileText);
        fileText = LoadFileText(TextFormat("%s/src/screen_ending.c", templatePath));
        SaveFileText(TextFormat("%s/%s/src/screen_ending.c", config->building.outputPath, outProjectName), fileText);
        UnloadFileText(fileText);
        
        LOG("INFO: Copied advance project with src/%s.c successfully\n", TextToLower(config->project.name));
    }
    else if (config->project.type == 2) // Use provided source files
    {
        for (int i = 0; i < config->project.srcFileCount; i++)
        {
            fileText = LoadFileText(config->project.srcFilePaths[i]);
            SaveFileText(TextFormat("%s/%s/src/%s", config->building.outputPath, outProjectName, GetFileName(config->project.srcFilePaths[i])), fileText);
            UnloadFileText(fileText);
            LOG("INFO: Copied src/%s successfully\n", GetFileName(config->project.srcFilePaths[i]));
        }
    }
    //--------------------------------------------------------------------------
    
    // Project build system: Script
    //-------------------------------------------------------------------------------------
    MakeDirectory(TextFormat("%s/%s/projects/scripts", config->building.outputPath, outProjectName));
    // Update src/build.bat (Windows only)
    // TODO: Use CMD/Shell calls directly, current script uses Makefile
    fileText = LoadFileText(TextFormat("%s/projects/scripts/build.bat", templatePath));
    fileTextUpdated[0] = TextReplace(fileText, "project_name", TextToLower(config->project.name));
    fileTextUpdated[1] = TextReplace(fileTextUpdated[0], "ProjectDescription", config->project.description);
    fileTextUpdated[2] = TextReplace(fileTextUpdated[1], "C:\\raylib\\w64devkit\\bin", config->building.compilerPath);
    SaveFileText(TextFormat("%s/%s/projects/scripts/build.bat", config->building.outputPath, outProjectName), fileTextUpdated[2]);
    for (int i = 0; i < 6; i++) { MemFree(fileTextUpdated[i]); fileTextUpdated[i] = NULL; }
    UnloadFileText(fileText);
    
    LOG("INFO: Updated build system successfully: Script (src/build.bat)\n");
    //-------------------------------------------------------------------------------------

    // Project build system: Makefile
    //-------------------------------------------------------------------------------------
    // Update src/Makefile
    // TODO: Update emscripten path for Web building
    fileText = LoadFileText(TextFormat("%s/src/Makefile", templatePath));
    if (config->project.type == 0) // Using basic template (one file)
    {
        fileTextUpdated[0] = TextReplace(fileText, "project_name.c", TextFormat("%s.c", TextToLower(config->project.name)));
    }
    else if (config->project.type == 1) // Using advance template (multiple files)
    {
        fileTextUpdated[0] = TextReplace(fileText, "project_name.c", TextFormat("%s.c screen_logo.c screen_title.c screen_options.c screen_gameplay.c screen_ending.c", TextToLower(config->project.name)));
    }
    else if (config->project.type == 2) // Using custom provided source files
    {
        char **srcFileNames = (char **)RL_CALLOC(256, sizeof(char *)); // Max number of input source files supported
        for (int i = 0; i < 256; i++) srcFileNames[i] = (char *)RL_CALLOC(256, sizeof(char));
        
        int codeFileCount = 0;
        for (int j = 0; j < config->project.srcFileCount; j++)
        {
            if (IsFileExtension(config->project.srcFilePaths[j], ".c"))
            {
                strcpy(srcFileNames[codeFileCount], GetFileName(config->project.srcFilePaths[j]));
                codeFileCount++;
            }
        }
        
        fileTextUpdated[0] = TextReplace(fileText, "project_name.c", TextJoin(srcFileNames, codeFileCount, " "));
        
        for (int i = 0; i < 256; i++) RL_FREE(srcFileNames[i]);
        RL_FREE(srcFileNames);
    }
    fileTextUpdated[1] = TextReplace(fileText, "project_name", TextToLower(config->project.name));
    fileTextUpdated[2] = TextReplace(fileTextUpdated[0], "C:\\raylib\\w64devkit\\bin", config->building.compilerPath);
    fileTextUpdated[3] = TextReplace(fileTextUpdated[1], "C:/raylib/raylib/src", config->building.raylibSrcPath);
    SaveFileText(TextFormat("%s/%s/src/Makefile", config->building.outputPath, outProjectName), fileTextUpdated[3]);
    
    for (int i = 0; i < 6; i++) { MemFree(fileTextUpdated[i]); fileTextUpdated[i] = NULL; }
    UnloadFileText(fileText);
    
    LOG("INFO: Updated build system successfully: Makefile (src/Makefile)\n");
    //-------------------------------------------------------------------------------------

    // Project build system: VS2022
    //-------------------------------------------------------------------------------------
    // Create required output directories
    MakeDirectory(TextFormat("%s/%s/projects/VS2022/raylib", config->building.outputPath, outProjectName));
    MakeDirectory(TextFormat("%s/%s/projects/VS2022/%s", config->building.outputPath, outProjectName, outProjectName));
    // Copy projects/VS2022/raylib/raylib.vcxproj
    fileText = LoadFileText(TextFormat("%s/projects/VS2022/raylib/raylib.vcxproj", templatePath));
    fileTextUpdated[0] = TextReplace(fileText, "C:\\raylib\\raylib\\src", config->building.raylibSrcPath);
    SaveFileText(TextFormat("%s/%s/projects/VS2022/raylib/raylib.vcxproj", config->building.outputPath, outProjectName, outProjectName), fileTextUpdated[0]);
    for (int i = 0; i < 6; i++) { MemFree(fileTextUpdated[i]); fileTextUpdated[i] = NULL; }
    UnloadFileText(fileText);

    // Copy projects/VS2022/raylib/Directory.Build.props
    //fileText = LoadFileText(TextFormat("%s/projects/VS2022/raylib/Directory.Build.props", templatePath));
    //SaveFileText(TextFormat("%s/%s/projects/VS2022/raylib/Directory.Build.props", config->building.outputPath, outProjectName, outProjectName), fileText);
    //UnloadFileText(fileText);

    // Update projects/VS2022/project_name/config->project_name.vcproj
    fileText = LoadFileText(TextFormat("%s/projects/VS2022/project_name/project_name.vcxproj", templatePath));
    if (config->project.type == 0) // Using basic template (one file)
    {
        fileTextUpdated[0] = TextReplace(fileText, "project_name.c", TextFormat("%s.c", TextToLower(config->project.name)));
        fileTextUpdated[1] = TextReplace(fileTextUpdated[0], "", "");
    }
    else if (config->project.type == 1) // Using advance template (multiple files)
    {
        fileTextUpdated[0] = TextReplace(fileText, "project_name.c", TextFormat("%s.c", TextToLower(config->project.name))); 
        fileTextUpdated[1] = TextReplace(fileTextUpdated[0], "<!--Additional Compile Items-->", 
        "<ClCompile Include=\"..\\..\\..\\src\\screen_logo.c\" />\n    \
         <ClCompile Include=\"..\\..\\..\\src\\screen_title.c\" />\n    \
         <ClCompile Include=\"..\\..\\..\\src\\screen_options.c\" />\n    \
         <ClCompile Include=\"..\\..\\..\\src\\screen_gameplay.c\" />\n    \
         <ClCompile Include=\"..\\..\\..\\src\\screen_ending.c\" />\n");
    }
    else if (config->project.type == 2) // Using custom provided source files
    {
        char **srcFileNames = (char **)RL_CALLOC(256, sizeof(char *)); // Max number of input source files supported
        for (int i = 0; i < 256; i++) srcFileNames[i] = (char *)RL_CALLOC(256, sizeof(char));
        
        int codeFileCount = 0;
        for (int j = 0; j < config->project.srcFileCount; j++)
        {
            if (IsFileExtension(config->project.srcFilePaths[j], ".c"))
            {
                strcpy(srcFileNames[codeFileCount], GetFileName(config->project.srcFilePaths[j]));
                codeFileCount++;
            }
        }
        
        fileTextUpdated[0] = TextReplace(fileText, "project_name.c", srcFileNames[0]);
        char srcFilesBlock[1024] = { 0 };
        int nextPosition = 0;
        for (int k = 1; k < codeFileCount; k++)
        {
            TextAppend(srcFilesBlock, TextFormat("<ClCompile Include=\"..\\..\\..\\src\\%s\" />\n    ", srcFileNames[k]), &nextPosition);
        }
        
        fileTextUpdated[1] = TextReplace(fileTextUpdated[0], "<!--Additional Compile Items-->", srcFilesBlock);
        
        for (int i = 0; i < 256; i++) RL_FREE(srcFileNames[i]);
        RL_FREE(srcFileNames);
    }
    fileTextUpdated[2] = TextReplace(fileTextUpdated[1], "project_name", outProjectName);
    fileTextUpdated[3] = TextReplace(fileTextUpdated[2], "C:\\raylib\\raylib\\src", config->building.raylibSrcPath);
    SaveFileText(TextFormat("%s/%s/projects/VS2022/%s/%s.vcxproj", config->building.outputPath, outProjectName, outProjectName, outProjectName), fileTextUpdated[3]);
    for (int i = 0; i < 6; i++) { MemFree(fileTextUpdated[i]); fileTextUpdated[i] = NULL; }
    UnloadFileText(fileText);

    // Update projects/VS2022/project_name.sln
    fileText = LoadFileText(TextFormat("%s/projects/VS2022/project_name.sln", templatePath));
    fileTextUpdated[0] = TextReplace(fileText, "project_name", outProjectName);
    SaveFileText(TextFormat("%s/%s/projects/VS2022/%s.sln", config->building.outputPath, outProjectName, outProjectName), fileTextUpdated[0]);
    for (int i = 0; i < 6; i++) { MemFree(fileTextUpdated[i]); fileTextUpdated[i] = NULL; }
    UnloadFileText(fileText);

    LOG("INFO: Updated build system successfully: VS2022 (projects/VS2022)\n");
    //-------------------------------------------------------------------------------------
    
    // Project build system: VSCode
    //-------------------------------------------------------------------------------------
    // Create required output directories
    MakeDirectory(TextFormat("%s/%s/projects/VSCode/.vscode", config->building.outputPath, outProjectName));
    // Update projects/VSCode/.vscode/launch.json
    fileText = LoadFileText(TextFormat("%s/projects/VSCode/.vscode/launch.json", templatePath));
    fileTextUpdated[0] = TextReplace(fileText, "project_name", TextToLower(config->project.name));
    fileTextUpdated[1] = TextReplace(fileTextUpdated[0], "C:/raylib/w64devkit/bin", config->building.compilerPath);
    SaveFileText(TextFormat("%s/%s/projects/VSCode/.vscode/launch.json", config->building.outputPath, outProjectName), fileTextUpdated[1]);
    for (int i = 0; i < 6; i++) { MemFree(fileTextUpdated[i]); fileTextUpdated[i] = NULL; }
    UnloadFileText(fileText);
    
    // Update projects/VSCode/.vscode/c_cpp_properties.json
    fileText = LoadFileText(TextFormat("%s/projects/VSCode/.vscode/c_cpp_properties.json", templatePath));
    fileTextUpdated[0] = TextReplace(fileText, "C:/raylib/raylib/src", config->building.raylibSrcPath);
    fileTextUpdated[1] = TextReplace(fileTextUpdated[0], "C:/raylib/w64devkit/bin", config->building.compilerPath);
    SaveFileText(TextFormat("%s/%s/projects/VSCode/.vscode/c_cpp_properties.json", config->building.outputPath, outProjectName), fileTextUpdated[1]);
    for (int i = 0; i < 6; i++) { MemFree(fileTextUpdated[i]); fileTextUpdated[i] = NULL; }
    UnloadFileText(fileText);
    
    // Update projects/VSCode/.vscode/tasks.json
    fileText = LoadFileText(TextFormat("%s/projects/VSCode/.vscode/tasks.json", templatePath));
    
    // Update source code files
    if (config->project.type == 0) // Using basic template (one file)
    {
        fileTextUpdated[0] = TextReplace(fileText, "project_name.c", TextFormat("%s.c", TextToLower(config->project.name)));
    }
    else if (config->project.type == 1) // Using advance template (multiple files)
    {
        fileTextUpdated[0] = TextReplace(fileText, "project_name.c", TextFormat("%s.c screen_logo.c screen_title.c screen_options.c screen_gameplay.c screen_ending.c", TextToLower(config->project.name)));
    }
    else if (config->project.type == 2) // Using custom provided source files
    {
        char **srcFileNames = (char **)RL_CALLOC(256, sizeof(char *)); // Max number of input source files supported
        for (int i = 0; i < 256; i++) srcFileNames[i] = (char *)RL_CALLOC(256, sizeof(char));
        
        int codeFileCount = 0;
        for (int j = 0; j < config->project.srcFileCount; j++)
        {
            if (IsFileExtension(config->project.srcFilePaths[j], ".c"))
            {
                strcpy(srcFileNames[codeFileCount], GetFileName(config->project.srcFilePaths[j]));
                codeFileCount++;
            }
        }
        
        fileTextUpdated[0] = TextReplace(fileText, "project_name.c", TextJoin(srcFileNames, codeFileCount, " "));
        
        for (int i = 0; i < 256; i++) RL_FREE(srcFileNames[i]);
        RL_FREE(srcFileNames);
    }

    fileTextUpdated[1] = TextReplace(fileText, "project_name", TextToLower(config->project.name));
    fileTextUpdated[2] = TextReplace(fileTextUpdated[1], "C:/raylib/raylib/src", config->building.raylibSrcPath);
    fileTextUpdated[3] = TextReplace(fileTextUpdated[2], "C:/raylib/w64devkit/bin", config->building.compilerPath);
    
    SaveFileText(TextFormat("%s/%s/projects/VSCode/.vscode/tasks.json", config->building.outputPath, outProjectName), fileTextUpdated[3]);
    for (int i = 0; i < 6; i++) { MemFree(fileTextUpdated[i]); fileTextUpdated[i] = NULL; }
    UnloadFileText(fileText);
    
    // Copy projects/VSCode/.vscode/settings.json
    fileText = LoadFileText(TextFormat("%s/projects/VSCode/.vscode/settings.json", templatePath));
    SaveFileText(TextFormat("%s/%s/projects/VSCode/.vscode/settings.json", config->building.outputPath, outProjectName, outProjectName), fileText);
    UnloadFileText(fileText);
    
    // Copy projects/VSCode/main.code-workspace
    fileText = LoadFileText(TextFormat("%s/projects/VSCode/main.code-workspace", templatePath));
    SaveFileText(TextFormat("%s/%s/projects/VSCode/main.code-workspace", config->building.outputPath, outProjectName, outProjectName), fileText);
    UnloadFileText(fileText);
    
    // Copy projects/VSCode/README.md
    fileText = LoadFileText(TextFormat("%s/projects/VSCode/README.md", templatePath));
    SaveFileText(TextFormat("%s/%s/projects/VSCode/README.md", config->building.outputPath, outProjectName, outProjectName), fileText);
    UnloadFileText(fileText);
    
    LOG("INFO: Updated build system successfully: VSCode (projects/VSCode)\n");
    //-------------------------------------------------------------------------------------
    
    // Project build system: GitHub Actions
    // - Windows: Uses VS2022 project
    // - Linux, macOS, WebAssembly: Uses Makefile project
    // WARNING: Expects the PROJECT_NAME to be the repository-name (as generated by default)
    //-------------------------------------------------------------------------------------
    // Create required output directories
    MakeDirectory(TextFormat("%s/%s/.github/workflows", config->building.outputPath, outProjectName));
    // Copy GitHub workflows: linux.yml, webassembly.yml, windows.yml
    fileText = LoadFileText(TextFormat("%s/.github/workflows/windows.yml", templatePath));
    SaveFileText(TextFormat("%s/%s/.github/workflows/windows.yml", config->building.outputPath, outProjectName, outProjectName), fileText);
    UnloadFileText(fileText);
    fileText = LoadFileText(TextFormat("%s/.github/workflows/linux.yml", templatePath));
    SaveFileText(TextFormat("%s/%s/.github/workflows/linux.yml", config->building.outputPath, outProjectName, outProjectName), fileText);
    UnloadFileText(fileText);
    fileText = LoadFileText(TextFormat("%s/.github/workflows/macos.yml", templatePath));
    SaveFileText(TextFormat("%s/%s/.github/workflows/macos.yml", config->building.outputPath, outProjectName, outProjectName), fileText);
    UnloadFileText(fileText);
    fileText = LoadFileText(TextFormat("%s/.github/workflows/webassembly.yml", templatePath));
    SaveFileText(TextFormat("%s/%s/.github/workflows/webassembly.yml", config->building.outputPath, outProjectName, outProjectName), fileText);
    UnloadFileText(fileText);
    
    LOG("INFO: Updated build system successfully: GitHub Actions CI/CD workflows (.github)\n");
    //-------------------------------------------------------------------------------------

    // Update additional files required for product building
    //  - src/project_name.rc   -> Windows: Executable resource file, includes .ico and metadata
    //  - src/project_name.ico  -> Product icon, required for Window resource file
    //  - src/project_name.icns -> macOS: Product icon, required by Info.plist
    //  - src/Info.plist        -> macOS application resource file, includes .icns and metadata
    //  - src/minshell.html     -> Web: Html minimum shell for WebAssembly application, preconfigured
    //-------------------------------------------------------------------------------------
    // Update src/project_name.rc
    fileText = LoadFileText(TextFormat("%s/src/project_name.rc", templatePath));
    fileTextUpdated[0] = TextReplace(fileText, "ProductName", config->project.product);
    fileTextUpdated[1] = TextReplace(fileTextUpdated[0], "project_name", TextToLower(config->project.name));
    fileTextUpdated[2] = TextReplace(fileTextUpdated[1], "ProjectDescription", config->project.description);
    fileTextUpdated[3] = TextReplace(fileTextUpdated[2], "ProjectDev", config->project.developer);
    SaveFileText(TextFormat("%s/%s/src/%s.rc", config->building.outputPath, outProjectName, outProjectName), fileTextUpdated[3]);
    for (int i = 0; i < 6; i++) { MemFree(fileTextUpdated[i]); fileTextUpdated[i] = NULL; }
    UnloadFileText(fileText);
    LOG("INFO: Updated src/%s.rc successfully\n", outProjectName);

    // Copy src/project_name.ico to src/project_name.ico
    int fileDataSize = 0;
    unsigned char *fileData = LoadFileData(TextFormat("%s/src/project_name.ico", templatePath), &fileDataSize);
    SaveFileData(TextFormat("%s/%s/src/%s.ico", config->building.outputPath, outProjectName, outProjectName), fileData, fileDataSize);
    UnloadFileData(fileData);
    LOG("INFO: Copied src/%s.ico successfully\n", TextToLower(config->project.name));

    // Copy src/project_name.icns to src/project_name.icns
    fileDataSize = 0;
    fileData = LoadFileData(TextFormat("%s/src/project_name.icns", templatePath), &fileDataSize);
    SaveFileData(TextFormat("%s/%s/src/%s.icns", config->building.outputPath, outProjectName, outProjectName), fileData, fileDataSize);
    UnloadFileData(fileData);
    LOG("INFO: Copied src/%s.icns successfully\n", TextToLower(config->project.name));

    // Update src/Info.plist
    fileText = LoadFileText(TextFormat("%s/src/Info.plist", templatePath));
    fileTextUpdated[0] = TextReplace(fileText, "ProductName", config->project.product);
    fileTextUpdated[1] = TextReplace(fileTextUpdated[0], "project_name", TextToLower(config->project.name));
    fileTextUpdated[2] = TextReplace(fileTextUpdated[1], "ProjectDescription", config->project.description);
    fileTextUpdated[3] = TextReplace(fileTextUpdated[2], "ProjectDev", config->project.developer);
    fileTextUpdated[4] = TextReplace(fileTextUpdated[3], "project_dev", TextToLower(config->project.developer));
    fileTextUpdated[5] = TextReplace(fileTextUpdated[4], "developer_web", TextToLower(config->project.developerWeb));
    SaveFileText(TextFormat("%s/%s/src/Info.plist", config->building.outputPath, outProjectName), fileTextUpdated[5]);
    for (int i = 0; i < 6; i++) { MemFree(fileTextUpdated[i]); fileTextUpdated[i] = NULL; }
    UnloadFileText(fileText);
    LOG("INFO: Updated src/Info.plist successfully\n");

    // Update src/minshell.html
    // TODO: Review Webpage, links, OpenGraph/X card, keywords...
    fileText = LoadFileText(TextFormat("%s/src/minshell.html", templatePath));
    fileTextUpdated[0] = TextReplace(fileText, "ProductName", config->project.product);
    fileTextUpdated[1] = TextReplace(fileTextUpdated[0], "project_name", TextToLower(config->project.name));
    fileTextUpdated[2] = TextReplace(fileTextUpdated[1], "ProjectDescription", config->project.description);
    fileTextUpdated[3] = TextReplace(fileTextUpdated[2], "ProjectDev", config->project.developer);
    fileTextUpdated[4] = TextReplace(fileTextUpdated[3], "project_dev", TextToLower(config->project.developer));
    fileTextUpdated[5] = TextReplace(fileTextUpdated[4], "developer_web", TextToLower(config->project.developerWeb));
    SaveFileText(TextFormat("%s/%s/src/minshell.html", config->building.outputPath, outProjectName), fileTextUpdated[5]);
    for (int i = 0; i < 6; i++) { MemFree(fileTextUpdated[i]); fileTextUpdated[i] = NULL; }
    UnloadFileText(fileText);
    LOG("INFO: Updated src/minshell.html successfully\n");
    //-------------------------------------------------------------------------------------

    // TODO: Process resource directory --> Copy to provided output resource path?

    // Update README.md
    fileText = LoadFileText(TextFormat("%s/README.md", templatePath));
    fileTextUpdated[0] = TextReplace(fileText, "ProductName", config->project.product);
    fileTextUpdated[1] = TextReplace(fileTextUpdated[0], "project_name", TextToLower(config->project.name));
    fileTextUpdated[2] = TextReplace(fileTextUpdated[1], "ProjectDescription", config->project.description);
    fileTextUpdated[3] = TextReplace(fileTextUpdated[2], "ProjectDev", config->project.developer);
    SaveFileText(TextFormat("%s/%s/README.md", config->building.outputPath, outProjectName), fileTextUpdated[3]);
    for (int i = 0; i < 6; i++) { MemFree(fileTextUpdated[i]); fileTextUpdated[i] = NULL; }
    UnloadFileText(fileText);
    LOG("INFO: Updated README.md successfully\n");

    // Update LICENSE, including ProjectDev
    fileText = LoadFileText(TextFormat("%s/LICENSE", templatePath));
    fileTextUpdated[0] = TextReplace(fileText, "ProjectDev", config->project.developer);
    SaveFileText(TextFormat("%s/%s/LICENSE", config->building.outputPath, outProjectName, outProjectName), fileTextUpdated[0]);
    for (int i = 0; i < 6; i++) { MemFree(fileTextUpdated[i]); fileTextUpdated[i] = NULL; }
    UnloadFileText(fileText);
    LOG("INFO: Updated LICENSE successfully\n");
    
    // Copy from template files that do not require customization: CONVENTIONS.md, .gitignore
    fileText = LoadFileText(TextFormat("%s/CONVENTIONS.md", templatePath));
    SaveFileText(TextFormat("%s/%s/CONVENTIONS.md", config->building.outputPath, outProjectName, outProjectName), fileText);
    UnloadFileText(fileText);
    fileText = LoadFileText(TextFormat("%s/.gitignore", templatePath));
    SaveFileText(TextFormat("%s/%s/.gitignore", config->building.outputPath, outProjectName, outProjectName), fileText);
    UnloadFileText(fileText);

    LOG("INFO: GitHub %s project generated successfully!\n", config->project.name);
}

// Packing of directory files into a binary blob
static char *PackDirectoryData(const char *baseDirPath, int *packSize)
{
    #define MAX_PACKED_DATA_SIZE    10*1024*1024    // 10 MB
    
    int fullPackSize = 0;
    char *data = NULL;

    FilePathList files = LoadDirectoryFilesEx(baseDirPath, NULL, true);
    
    if (files.count > 0)
    {
        int filesDataSize = 0;
        data = (char *)RL_CALLOC(MAX_PACKED_DATA_SIZE, 1);
        PackFileEntry *entries = (PackFileEntry *)RL_CALLOC(files.count, sizeof(PackFileEntry));
        
        for (unsigned int i = 0; i < files.count; i++) 
        {
            strcpy(entries[i].filePath, files.paths[i]);
            unsigned char *fileData = LoadFileData(files.paths[i], &entries[i].fileSize);
            unsigned char *compFileData = CompressData(fileData, entries[i].fileSize, &entries[i].compFileSize);
            UnloadFileData(fileData);
            
            printf("Packing file: %s\n", files.paths[i]);

            memcpy(data + filesDataSize, compFileData, entries[i].compFileSize);
            filesDataSize += entries[i].compFileSize;
            MemFree(compFileData);
        }
        
        // OPTION: Compress entries data for optimization
        int compEntriesDataSize = 0;
        unsigned char *compEntriesData = CompressData((unsigned char *)entries, files.count*sizeof(PackFileEntry), &compEntriesDataSize);
        
        // Append entries data + compEntriesDataSize + filesDataSize + files.count + CCFOUR (at the end of file)
        fullPackSize = filesDataSize;
        memcpy(data + fullPackSize, compEntriesData, compEntriesDataSize);
        fullPackSize += compEntriesDataSize;
        
        memcpy(data + fullPackSize, &compEntriesDataSize, sizeof(int));
        fullPackSize += sizeof(int);
        memcpy(data + fullPackSize, &filesDataSize, sizeof(int));
        fullPackSize += sizeof(int);
        memcpy(data + fullPackSize, &files.count, sizeof(int));
        fullPackSize += sizeof(int);
        char fourcc[5] = "rpch";
        memcpy(data + fullPackSize, fourcc, 4);
        fullPackSize += 4;
    }

    UnloadDirectoryFiles(files);
    
    *packSize = fullPackSize;
    return data;
}

// Unpacking of directory files from a binary blob
static void UnpackDirectoryData(const char *outputDirPath, const unsigned char *data, int *dataSize, PackFileEntry *entries, int fileCount)
{
    int nextFileDataOffset = 0;
    
    for (int i = 0; i < fileCount; i++)
    {    
        // Decompress entry from data
        int fileDataSize = 0;
        unsigned char *fileData = DecompressData(data + nextFileDataOffset, entries[i].compFileSize, &fileDataSize);
        
        // Verify process worked as expected
        if ((fileData != NULL) && (fileDataSize == entries[i].fileSize))
        {
            SaveFileData(TextFormat("%s/%s", outputDirPath, entries[i].filePath), fileData, fileDataSize);
        }
        else
        {
            LOG("WARNING: File data could not be decompressed!\n");
            break;
        }
        
        MemFree(fileData);
        nextFileDataOffset += entries[i].compFileSize;
    }
}

// Load a text file data from memory packed data
char *LoadFileTextPack(const char *fileName, const char *packData, PackFileEntry *entries, int fileCount)
{
    int fileDataSize = 0;
    char *fileData = NULL;
    
    // Find data offset in package and decompress it
    for (int i = 0, dataOffset = 0; i < fileCount; i++)
    {
        if (TextIsEqual(fileName, entries[i].filePath))
        {
            unsigned char *uncompFileData = DecompressData(packData + dataOffset, entries[i].compFileSize, &fileDataSize);
            
            if ((fileData != NULL) && (fileDataSize == entries[i].fileSize)) 
            {
                // NOTE: We make sure the text data ends with /0
                fileData = (char *)RL_CALLOC(entries[i].fileSize + 1, 1);
                memcpy(fileData, uncompFileData, fileDataSize);
                MemFree(uncompFileData);
            }
            else LOG("WARNING: File not loaded properly from pack\n");
            
            break;
        }
        else dataOffset += entries[i].compFileSize;
    }
    
    return fileData;
}

// Split string into multiple strings
// NOTE: No memory is dynamically allocated
static const char **GetSubtextPtrs(char *text, char delimiter, int *count)
{
    #define MAX_SUBTEXTPTRS_COUNT    1024

    // WARNING: Input string is modified, '\0' is added in the delimiter and the resulting strings
    // are returned as a static array of pointers
    // Maximum number of pointed strings is set by MAX_TEXTSPLIT_COUNT
    static const char *result[MAX_SUBTEXTPTRS_COUNT] = { NULL };

    result[0] = text;
    int counter = 1;
    int textSize = TextLength(text);

    if (text != NULL)
    {
        // Count how many substrings we have on text and point to every one
        for (int i = 0; i < textSize; i++)
        {
            if (text[i] == '\0') break;
            else if (text[i] == delimiter)
            {
                text[i] = '\0';   // Set an end of string at this point
                result[counter] = text + i + 1;
                counter++;

                if (counter == MAX_SUBTEXTPTRS_COUNT) break;
            }
        }
    }

    *count = counter;
    return result;
}