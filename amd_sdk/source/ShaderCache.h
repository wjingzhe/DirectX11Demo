//
// Copyright (c) 2016 Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

//--------------------------------------------------------------------------------------
// File: ShaderCache.h
//
// Class definition for the ShaderCache interface. Allows the user to add shaders to a list
// which is then compiled in parallel to object files. Future calls to create the shaders,
// will simply re-use the object files, making craetion time very fast. The option is there,
// to force the regeneration of object files.
//
// Assumption, relies on following directory structure:
//
// SolutionDir\..\src\Shaders
//--------------------------------------------------------------------------------------
#ifndef AMD_SDK_SHADER_CACHE_H
#define AMD_SDK_SHADER_CACHE_H

#include <set>
#include <list>
#include <vector>



namespace AMD
{

    class ShaderCache
    {
    public:

        // Constants used for string size limits
        static const int m_uCOMMAND_LINE_MAX_LENGTH = 2048;
        static const int m_uTARGET_MAX_LENGTH = 16;
        static const int m_uENTRY_POINT_MAX_LENGTH = 128;
        static const int m_uFILENAME_MAX_LENGTH = 256;
        static const int m_uPATHNAME_MAX_LENGTH = 512;
        static const int m_uMACRO_MAX_LENGTH = 64;

        // Shader type enumeration
        typedef enum SHADER_TYPE_t
        {
            SHADER_TYPE_VERTEX,
            SHADER_TYPE_HULL,
            SHADER_TYPE_DOMAIN,
            SHADER_TYPE_GEOMETRY,
            SHADER_TYPE_PIXEL,
            SHADER_TYPE_COMPUTE,
            SHADER_TYPE_UNKNOWN,
            SHADER_TYPE_MAX
        }SHADER_TYPE;

        // Create type enumeration
        typedef enum CREATE_TYPE_t
        {
            CREATE_TYPE_FORCE_COMPILE,      // Clean the cache, and compile all
            CREATE_TYPE_COMPILE_CHANGES,    // Only compile shaders that have changed (development mode)
            CREATE_TYPE_USE_CACHED,         // Use cached shaders (release mode)
            CREATE_TYPE_MAX
        }CREATE_TYPE;

        // Shader auto-recompile type enumeration
        typedef enum SHADER_AUTO_RECOMPILE_TYPE_t
        {
            SHADER_AUTO_RECOMPILE_DISABLED, // Shaders are only checked at startup
            SHADER_AUTO_RECOMPILE_ENABLED,  // Auto-recompile changed shaders to improve iteration times
            SHADER_AUTO_RECOMPILE_MAX
        }SHADER_AUTO_RECOMPILE_TYPE;

        // Shader compiler error display type enumeration
        typedef enum ERROR_DISPLAY_TYPE_t
        {
            ERROR_DISPLAY_IN_DEBUG_OUTPUT,  // Just write shader compiler errors out to debug text
            ERROR_DISPLAY_ON_SCREEN,        // Render shader compiler errors to the screen
            ERROR_DISPLAY_IN_MESSAGE_BOX,   // Pop up a message box on compiler error
            ERROR_DISPLAY_IN_DEBUG_OUTPUT_AND_BREAK,  // Cause a breakpoint exception on compiler error
            ERROR_DISPLAY_TYPE_MAX
        }ERROR_DISPLAY_TYPE;

        // ISA generation type enumeration
        typedef enum GENERATE_ISA_TYPE_t
        {
            GENERATE_ISA_DISABLED,          // Don't generate ISA (this option is required when releasing samples)
            GENERATE_ISA_ENABLED,           // Generate ISA and gather info on GPR pressure (requires AMD internal ScDev)
            GENERATE_ISA_MAX
        }GENERATE_ISA_TYPE;

        // Shader compiler exe location type enumeration
        typedef enum SHADER_COMPILER_EXE_TYPE_t
        {
            SHADER_COMPILER_EXE_INSTALLED,  // Look for shader compiler EXEs (fxc and ScDev) in their installed locations
            SHADER_COMPILER_EXE_LOCAL,      // Look for shader compiler EXEs (fxc and ScDev) in the AMD_SDK\src\Shaders directory
                                            // (don't use LOCAL when releasing samples, as distributing fxc in this way violates the license)
            SHADER_COMPILER_EXE_MAX
        }SHADER_COMPILER_EXE_TYPE;

        // Max cores type enumeration
        typedef enum MAXCORES_TYPE_t
        {
            MAXCORES_NO_LIMIT           = -4,
            MAXCORES_2X_CPU_CORES       = -3,
            MAXCORES_USE_ALL_CORES      = -2,
            MAXCORES_USE_ALL_BUT_ONE    = -1,
            MAXCORES_MULTI_THREADED     =  0,
            MAXCORES_SINGLE_THREADED    =  1
        } MAXCORES_TYPE;

        // The Macro structure
        class Macro
        {
        public:

            wchar_t             m_wsName[m_uMACRO_MAX_LENGTH];
            int                 m_iValue;
        };

        // The shader class
        class Shader
        {
        public:

            Shader();
            ~Shader();

            SHADER_TYPE                 m_eShaderType;
            ID3D11DeviceChild**         m_ppShader;
            ID3D11InputLayout**         m_ppInputLayout;
            D3D11_INPUT_ELEMENT_DESC*   m_pInputLayoutDesc;
            unsigned int                m_uNumDescElements;
            wchar_t                     m_wsTarget[m_uTARGET_MAX_LENGTH];
            wchar_t                     m_wsEntryPoint[m_uENTRY_POINT_MAX_LENGTH];
            wchar_t                     m_wsSourceFile[m_uFILENAME_MAX_LENGTH];
            wchar_t                     m_wsCanonicalName[m_uFILENAME_MAX_LENGTH];
            unsigned int                m_uNumMacros;
            Macro*                      m_pMacros;

            wchar_t                     m_wsRawFileName[m_uFILENAME_MAX_LENGTH];
            wchar_t                     m_wsHashedFileName[m_uFILENAME_MAX_LENGTH];
            wchar_t                     m_wsCompiledObjectFile[m_uFILENAME_MAX_LENGTH];
            wchar_t                     m_wsErrorFile[m_uFILENAME_MAX_LENGTH];
            wchar_t                     m_wsAssemblyFile[m_uFILENAME_MAX_LENGTH];
            wchar_t                     m_wsAssemblyFileWithHashedFilename[m_uFILENAME_MAX_LENGTH];
            wchar_t                     m_wsISAFile[m_uFILENAME_MAX_LENGTH];
            wchar_t                     m_wsPreprocessFile[m_uFILENAME_MAX_LENGTH];
            wchar_t                     m_wsHashFile[m_uFILENAME_MAX_LENGTH];
            wchar_t                     m_wsCommandLine[m_uCOMMAND_LINE_MAX_LENGTH];
            wchar_t                     m_wsISACommandLine[m_uCOMMAND_LINE_MAX_LENGTH];
            wchar_t                     m_wsPreprocessCommandLine[m_uCOMMAND_LINE_MAX_LENGTH];

            wchar_t                     m_wsObjectFile_with_ISA[m_uFILENAME_MAX_LENGTH];
            wchar_t                     m_wsPreprocessFile_with_ISA[m_uFILENAME_MAX_LENGTH];



            bool                        m_bBeingProcessed;
			//bool						m_pCompiled
            bool                        m_bShaderUpToDate;
            BYTE*                       m_pHash;
            long                        m_uHashLength;

            BYTE*                       m_pFilenameHash;
            long                        m_uFilenameHashLength;

            const wchar_t*              m_wsCompileStatus;
            int                         m_iCompileWaitCount;
            HANDLE                      m_hCompileProcessHandle;
            HANDLE                      m_hCompileThreadHandle;

            void SetupHashedFilename( void );
        };

        // Construction / destruction
        ShaderCache( const SHADER_AUTO_RECOMPILE_TYPE i_keAutoRecompileTouchedShadersType = SHADER_AUTO_RECOMPILE_DISABLED,
            const ERROR_DISPLAY_TYPE i_keErrorDisplayType = ERROR_DISPLAY_IN_DEBUG_OUTPUT_AND_BREAK,
            const GENERATE_ISA_TYPE i_keGenerateShaderISAType = GENERATE_ISA_DISABLED,
            const SHADER_COMPILER_EXE_TYPE i_keShaderCompilerExeType = SHADER_COMPILER_EXE_INSTALLED );
        ~ShaderCache();

        // Allows the user to add a shader to the cache
        bool AddShader( ID3D11DeviceChild** ppShader,
            SHADER_TYPE ShaderType,
            const wchar_t* pwsTarget,
            const wchar_t* pwsEntryPoint,
            const wchar_t* pwsSourceFile,
            unsigned int uNumMacros,
            Macro* pMacros,
            ID3D11InputLayout** ppInputLayout,
            const D3D11_INPUT_ELEMENT_DESC* pLayout,
            unsigned int uNumElements,
            const wchar_t* pwsCanonicalName = 0,
            const int i_iMaxVGPRLimit = -1,
            const int i_iMaxSGPRLimit = -1,
            const bool i_kbIsApplicationShader = true );

        // Allows the ShaderCache to add a new type of ISA Target version of all shaders to the cache
        bool CloneShaders( void );

        // Allows the user to generate shaders added to the cache
        HRESULT GenerateShaders( CREATE_TYPE CreateType, const bool i_kbRecreateShaders = false );

        const bool  HasErrorsToDisplay( void ) const;
        const bool  ShowShaderErrors( void ) const;
        const int   ShaderErrorDisplayType( void ) const;
        const bool  RecompileTouchedShaders( void ) const;

        void        SetMaximumCoresForShaderCompiler( const int ki_MaxCores = MAXCORES_NO_LIMIT );
        void        SetRecompileTouchedShadersFlag( const bool i_bRecompileWhenTouched );
        void        SetShowShaderErrorsFlag( const bool i_kbShowShaderErrors );



        // Renders runtime shader compiler errors from dynamically recompiled shaders
        void RenderShaderErrors( CDXUTTextHelper* g_pTxtHelper, int iFontHeight, DirectX::XMVECTOR FontColor, const unsigned int ki_FrameTimeout = 2500 );

        // Renders the progress of the shader generation
        void RenderProgress( CDXUTTextHelper* g_pTxtHelper, int iFontHeight, DirectX::XMVECTOR FontColor );

        // Renders the GPR usage for the shaders
        void RenderISAInfo( CDXUTTextHelper* g_pTxtHelper, int iFontHeight, DirectX::XMVECTOR FontColor, const Shader *i_pShaderCmp = NULL, wchar_t *o_wsGPRInfo = NULL );

        // User can enquire to see if shaders are ready
        bool ShadersReady(bool bWithoutPrintedProgress = false);

        // DXUT framework hook method (flags the shaders as needing creating)
        void OnDestroyDevice();

        // Called by app when WM_QUIT is posted, so that shader generation can be aborted
        void Abort();

        // Called by the app to override optimizations when compiling shaders in release mode
        void ForceDebugShaders( bool bForce ) { m_bForceDebugShaders = bForce; }

        // Do not call this function
        void GenerateShadersThreadProc();

    private:

        // Preprocessing, compilation, and creation methods
        void PreprocessShaders();
        void CompileShaders();
        void InvalidateShaders();

        HRESULT CreateShaders();
        BOOL PreprocessShader( Shader* pShader );
        BOOL CompileShader( Shader* pShader );
        HRESULT CreateShader( Shader* pShader );

        // Hash methods
        void StripPathInfoFromPreprocessFile( Shader* pShader, FILE* pFile, char* pFileBufDst, int iFileSize );
        BOOL CreateHashFromPreprocessFile( Shader* pShader );
        static void CreateHash( const char* data, int iFileSize, BYTE** hash, long* len );
        void WriteHashFile( Shader* pShader );
        BOOL CompareHash( Shader* pShader );
        bool CreateHashDigest( const std::list<Shader*>& i_ShaderList );

        // Watch methods (for automatic shader recompilation when changed)
        bool WatchDirectoryForChanges( void );
        static void __stdcall onDirectoryChangeEventTriggered( void* args, BOOLEAN /*timeout*/ );

        // Check methodss
        BOOL CheckFXC();
        BOOL CheckShaderSourceFileExist( Shader* pShader );
        BOOL CheckCompiledObjectFileExsist( Shader* pShader );//jingz
        BOOL CheckErrorFile( Shader* pShader, bool& io_bHasShaderCompilerError );
        BOOL IsAnError( FILE* pFile );



        // Prints the error message to debug output
        void PrintShaderErrors( FILE* pFile );

        // Various delete methods
        // LAYLANOTE: This code is horrid, it should be replaced by a single template function taking the type of file to delete.
        void DeleteFileByFilename( const wchar_t* pwsFile ) const;
        void DeleteErrorFiles();
        void DeleteErrorFile( Shader* pShader );
        void DeleteAssemblyFiles();
        void DeleteAssemblyFile( Shader* pShader );
        void DeleteObjectFiles();
        void DeleteObjectFile( Shader* pShader );
        void DeletePreprocessFiles();
        void DeletePreprocessFile( Shader* pShader );
        void DeleteHashFiles();
        void DeleteHashFile( Shader* pShader );

        // Helpers for Long Filename Support
        void InsertOutputFilenameIntoCommandLine( wchar_t *pwsCommandLine, const wchar_t* pwsFileName ) const;
        void InsertInputFilenameIntoCommandLine( wchar_t *pwsCommandLine, const wchar_t* pwsFileName ) const;
        template< size_t N >
        void CreateFullPathFromOutputFilename( wchar_t (&pwsPath)[N], const wchar_t* pwsFileName ) const
        {
            swprintf_s( pwsPath, L"%s\\%s", m_wsUnicodeWorkingDir, pwsFileName );
        }
        template< size_t N >
        void CreateFullPathFromInputFilename( wchar_t (&pwsPath)[N], const wchar_t* pwsFileName ) const
        {
            swprintf_s( pwsPath, L"%s\\%s", m_wsUnicodeShaderSourceDir, pwsFileName );
        }

        // Private data
        CREATE_TYPE             m_CreateType;
        MAXCORES_TYPE           m_MaxCoresType;
        unsigned int            m_uNumCPUCoresToUse;
        unsigned int            m_uNumCPUCores;
        bool                    m_bShadersCreated;
        bool                    m_bAbort;
        bool                    m_bPrintedProgress;
        std::list<Shader*>      m_ApplicationSourceShadersList;//将要使用源码编译的shader对象
        std::list<Shader*>      m_AllShadersList;//包含已编译好的对象和未进行编译处理的对象
        std::list<Shader*>      m_NeedToPreprocessList;
        std::list<Shader*>      m_HashList;
        std::list<Shader*>      m_NeedToCompileList;
        std::list<Shader*>      m_CompileCheckList;
        std::list<Shader*>      m_CreateShaderWithCompiledBufCodesList;//without Compiling,just create from the compiled object files or some other cache files
        std::set<Shader*>       m_ErrorList;


        struct ProgressInfo
        {
            ProgressInfo( void )
                : m_wsFilename( L"No Progress Info Structure: filename unknown" )
                , m_wsStatus( L"No Status" )
                , m_pShader( NULL )
            {}

            ProgressInfo( Shader* i_pShader, const wchar_t* ki_wsStatus = L"Initializing..." )
                : m_wsFilename( (i_pShader) ? i_pShader->m_wsRawFileName : L"Invalid Progress Info Structure: filename unknown" )
                , m_wsStatus( ki_wsStatus )
                , m_pShader( i_pShader )
            {}

            wchar_t*            m_wsFilename;
            const wchar_t*      m_wsStatus;
            Shader*             m_pShader;
        };

        ProgressInfo*           m_pProgressInfo;

        unsigned int            m_uProgressCounter;
        wchar_t                 m_wsFxcExePath[m_uPATHNAME_MAX_LENGTH];
        wchar_t                 m_wsDevExePath[m_uPATHNAME_MAX_LENGTH];
        wchar_t                 m_wsAmdSdkDir[m_uPATHNAME_MAX_LENGTH];
        wchar_t                 m_wsShaderSourceDir[m_uPATHNAME_MAX_LENGTH];
        wchar_t                 m_wsUnicodeShaderSourceDir[m_uPATHNAME_MAX_LENGTH];
        wchar_t                 m_wsWorkingDir[m_uPATHNAME_MAX_LENGTH];
        wchar_t                 m_wsUnicodeWorkingDir[m_uPATHNAME_MAX_LENGTH];
        wchar_t                 m_wsBatchWorkingDir[m_uPATHNAME_MAX_LENGTH];
        wchar_t                 m_wsSCDEVWorkingDir[m_uPATHNAME_MAX_LENGTH];
        wchar_t                 m_wsLastShaderError[m_uCOMMAND_LINE_MAX_LENGTH * 4];

        CRITICAL_SECTION        m_CompileShaders_CriticalSection;
        CRITICAL_SECTION        m_GenISA_CriticalSection;
        HANDLE                  m_watchHandle;
        HANDLE                  m_waitPoolHandle;
        unsigned int            m_shaderErrorRenderedCount;
        bool                    m_bRecompileTouchedShaders;
        bool                    m_bShowShaderErrors;
        bool                    m_bHasShaderErrorsToDisplay;

        bool                    m_bForceDebugShaders;
        bool                    m_bCreateHashDigest;

        ERROR_DISPLAY_TYPE      m_ErrorDisplayType;
    };

} // namespace AMD

#endif
