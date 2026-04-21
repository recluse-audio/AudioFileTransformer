#define MyAppName "AudioFileTransformer"
#define MyAppVersion "0.0.1"
#define MyAppPublisher "recluse-audio"
#define MyAppURL "https://recluse-audio.com"
#define VST3Source SourcePath + "\..\..\BUILD\AudioFileTransformer_artefacts\Release\VST3\AudioFileTransformer.vst3"
#define OutputDir SourcePath + "\BUILD"

[Setup]
AppId={208970C3-C360-5FD4-8CA6-E6AE4C32334C}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
DefaultDirName={autopf}\{#MyAppPublisher}\{#MyAppName}
UninstallFilesDir={app}
DefaultGroupName={#MyAppName}
DisableDirPage=yes
DisableProgramGroupPage=yes
OutputDir={#OutputDir}
OutputBaseFilename={#MyAppName}_v{#MyAppVersion}_Windows_Installer
Compression=lzma
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=admin
ArchitecturesInstallIn64BitMode=x64compatible

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "{#VST3Source}\*"; DestDir: "{commoncf}\VST3\{#MyAppName}.vst3"; Flags: recursesubdirs createallsubdirs ignoreversion

[Icons]
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"
