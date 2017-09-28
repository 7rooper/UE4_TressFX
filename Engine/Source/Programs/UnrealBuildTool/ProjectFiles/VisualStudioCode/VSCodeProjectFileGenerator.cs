﻿// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.IO;
using Tools.DotNETCommon;
using System.Diagnostics;

namespace UnrealBuildTool
{
	class VSCodeProjectFolder : MasterProjectFolder
	{
		public VSCodeProjectFolder(ProjectFileGenerator InitOwnerProjectFileGenerator, string InitFolderName)
			: base(InitOwnerProjectFileGenerator, InitFolderName)
		{
		}
	}

	class VSCodeProject : ProjectFile
	{
		public VSCodeProject(FileReference InitFilePath)
			: base(InitFilePath)
		{
		}

		public override bool WriteProjectFile(List<UnrealTargetPlatform> InPlatforms, List<UnrealTargetConfiguration> InConfigurations)
		{
			return true;
		}
	}

	class VSCodeProjectFileGenerator : ProjectFileGenerator
	{
		private DirectoryReference VSCodeDir;
		private UnrealTargetPlatform HostPlatform;
		private bool BuildingForDotNetCore;
		private string FrameworkExecutableExtension;
		private string FrameworkLibraryExtension = ".dll";

		private string MakeWorkspaceRelativePath(FileReference InRef)
		{
			if (InRef.IsUnderDirectory(UnrealBuildTool.RootDirectory))
			{
				return "${workspaceRoot}/" + InRef.MakeRelativeTo(UnrealBuildTool.RootDirectory).Replace('\\', '/');
			}
			else
			{
				return InRef.ToString().Replace('\\', '/');
			}
		}

		private string MakeWorkspaceRelativePath(DirectoryReference InRef)
		{
			if (InRef.IsUnderDirectory(UnrealBuildTool.RootDirectory))
			{
				return "${workspaceRoot}/" + InRef.MakeRelativeTo(UnrealBuildTool.RootDirectory).Replace('\\', '/');
			}
			else
			{
				return InRef.ToString().Replace('\\', '/');
			}
		}

		public VSCodeProjectFileGenerator(FileReference InOnlyGameProject)
			: base(InOnlyGameProject)
		{
			BuildingForDotNetCore = Environment.CommandLine.Contains("-dotnetcore");
			FrameworkExecutableExtension = BuildingForDotNetCore ? ".dll" : ".exe";
		}

		class JsonFile
		{
			public JsonFile()
			{
			}

			public void BeginRootObject()
			{
				BeginObject();
			}

			public void EndRootObject()
			{
				EndObject();
				if (TabString.Length > 0)
				{
					throw new Exception("Called EndRootObject before all objects and arrays have been closed");
				}
			}

			public void BeginObject(string Name = null)
			{
				string Prefix = Name == null ? "" : Quoted(Name) + ": ";
				Lines.Add(TabString + Prefix + "{");
				TabString += "\t";
			}

			public void EndObject()
			{
				Lines[Lines.Count - 1] = Lines[Lines.Count - 1].TrimEnd(',');
				TabString = TabString.Remove(TabString.Length - 1);
				Lines.Add(TabString + "},");
			}

			public void BeginArray(string Name = null)
			{
				string Prefix = Name == null ? "" : Quoted(Name) + ": ";
				Lines.Add(TabString + Prefix + "[");
				TabString += "\t";
			}

			public void EndArray()
			{
				Lines[Lines.Count - 1] = Lines[Lines.Count - 1].TrimEnd(',');
				TabString = TabString.Remove(TabString.Length - 1);
				Lines.Add(TabString + "],");
			}

			public void AddField(string Name, bool Value)
			{
				Lines.Add(TabString + Quoted(Name) + ": " + Value.ToString().ToLower() + ",");
			}

			public void AddField(string Name, string Value)
			{
				Lines.Add(TabString + Quoted(Name) + ": " + Quoted(Value) + ",");
			}

			public void AddUnnamedField(string Value)
			{
				Lines.Add(TabString + Quoted(Value) + ",");
			}

			public void Write(FileReference File)
			{
				Lines[Lines.Count - 1] = Lines[Lines.Count - 1].TrimEnd(',');
				FileReference.WriteAllLines(File, Lines.ToArray());
			}

			private string Quoted(string Value)
			{
				return "\"" + Value + "\"";
			}

			private List<string> Lines = new List<string>();
			private string TabString = "";
		}

		override public string ProjectFileExtension
		{
			get
			{
				return ".vscode";
			}
		}

		public override void CleanProjectFiles(DirectoryReference InMasterProjectDirectory, string InMasterProjectName, DirectoryReference InIntermediateProjectFilesPath)
		{
		}

		public override bool ShouldGenerateIntelliSenseData()
		{
			return true;
		}

		protected override ProjectFile AllocateProjectFile(FileReference InitFilePath)
		{
			return new VSCodeProject(InitFilePath);
		}

		public override MasterProjectFolder AllocateMasterProjectFolder(ProjectFileGenerator InitOwnerProjectFileGenerator, string InitFolderName)
		{
			return new VSCodeProjectFolder(InitOwnerProjectFileGenerator, InitFolderName);
		}

		private void GatherBuildProducts(ProjectFile Project, List<BuildProduct> OutToBuild)
		{
			if (Project is VSCodeProject)
			{
				foreach (ProjectTarget Target in Project.ProjectTargets)
				{
					string ProjectName = Target.TargetRules.Name;

					List<UnrealTargetConfiguration> Configs = new List<UnrealTargetConfiguration>();
					List<UnrealTargetPlatform> Platforms = new List<UnrealTargetPlatform>();
					Target.TargetRules.GetSupportedConfigurations(ref Configs, true);
					Target.TargetRules.GetSupportedPlatforms(ref Platforms);

					foreach (UnrealTargetPlatform Platform in Platforms)
					{
						if (SupportedPlatforms.Contains(Platform))
						{
							foreach (UnrealTargetConfiguration Config in Configs)
							{
								OutToBuild.Add(new BuildProduct { ProjectName = ProjectName, Platform = Platform, Config = Config, Type = Target.TargetRules.Type });
							}
						}
					}
				}
			}
			else if (Project is VCSharpProjectFile)
			{
				string ProjectName = Project.ProjectFilePath.GetFileNameWithoutExtension();

				VCSharpProjectFile VCSharpProject = Project as VCSharpProjectFile;
				UnrealTargetConfiguration[] Configs = { UnrealTargetConfiguration.Debug, UnrealTargetConfiguration.Development };

				foreach (UnrealTargetConfiguration Config in Configs)
				{
					CsProjectInfo Info = VCSharpProject.GetProjectInfo(Config);

					if (!Info.IsDotNETCoreProject() && Info.Properties.ContainsKey("OutputPath"))
					{
						OutToBuild.Add(new BuildProduct { ProjectName = ProjectName, Platform = HostPlatform, Config = Config, Type = TargetType.Program });
					}
				}
			}
		}

		protected override bool WriteMasterProjectFile(ProjectFile UBTProject)
		{
			VSCodeDir = DirectoryReference.Combine(MasterProjectPath, ".vscode");
			DirectoryReference.CreateDirectory(VSCodeDir);

			HostPlatform = BuildHostPlatform.Current.Platform;

			List<ProjectFile> Projects = new List<ProjectFile>(AllProjectFiles);
			Projects.Sort((A, B) => { return A.ProjectFilePath.GetFileName().CompareTo(B.ProjectFilePath.GetFileName()); });

			ProjectData ProjectData = GatherProjectData(Projects);

			WriteTasksFile(ProjectData);
			WriteLaunchFile(ProjectData);
			WriteWorkspaceSettingsFile(Projects);
			WriteCppPropertiesFile(ProjectData);
			//WriteProjectDataFile(ProjectData);

			return true;
		}

		private class ProjectData
		{
			public enum EOutputType
			{
				Library,
				Exe,

				WinExe, // some projects have this so we need to read it, but it will be converted across to Exe so no code should handle it!
			}

			public class BuildProduct
			{
				public FileReference OutputFile { get; set; }
				public UnrealTargetConfiguration Config { get; set; }
				public UnrealTargetPlatform Platform { get; set; }
				public EOutputType OutputType { get; set; }

				public CsProjectInfo CSharpInfo { get; set; }

				public override string ToString()
				{
					return Platform.ToString() + " " + Config.ToString(); 
				}

			}

			public class Target
			{
				public string Name;
				public TargetType Type;
				public List<BuildProduct> BuildProducts = new List<BuildProduct>();
				public List<string> Defines = new List<string>();

				public Target(Project InParentProject, string InName, TargetType InType)
				{
					Name = InName;
					Type = InType;
					InParentProject.Targets.Add(this);
				}

				public override string ToString()
				{
					return Name.ToString() + " " + Type.ToString(); 
				}
			}

			public class Project
			{
				public string Name;
				public ProjectFile SourceProject;
				public List<Target> Targets = new List<Target>();
				public List<string> IncludePaths;

				public override string ToString()
				{
					return Name;
				}
			}

			public List<Project> NativeProjects = new List<Project>();
			public List<Project> CSharpProjects = new List<Project>();
			public List<Project> AllProjects = new List<Project>();
		}

		private ProjectData GatherProjectData(List<ProjectFile> InProjects)
		{
			ProjectData ProjectData = new ProjectData();

			foreach (ProjectFile Project in InProjects)
			{
				// Create new project record
				ProjectData.Project NewProject = new ProjectData.Project();
				NewProject.Name = Project.ProjectFilePath.GetFileNameWithoutExtension();
				NewProject.SourceProject = Project;

				ProjectData.AllProjects.Add(NewProject);

				// Add into the correct easy-access list
				if (Project is VSCodeProject)
				{
					foreach (ProjectTarget Target in Project.ProjectTargets)
					{
						TargetRules.CPPEnvironmentConfiguration CppEnvironment = new TargetRules.CPPEnvironmentConfiguration(Target.TargetRules);
						TargetRules.LinkEnvironmentConfiguration LinkEnvironment = new TargetRules.LinkEnvironmentConfiguration(Target.TargetRules);
						Target.TargetRules.SetupGlobalEnvironment(new TargetInfo(new ReadOnlyTargetRules(Target.TargetRules)), ref LinkEnvironment, ref CppEnvironment);

						List<UnrealTargetConfiguration> Configs = new List<UnrealTargetConfiguration>();
						List<UnrealTargetPlatform> Platforms = new List<UnrealTargetPlatform>();
						Target.TargetRules.GetSupportedConfigurations(ref Configs, true);
						Target.TargetRules.GetSupportedPlatforms(ref Platforms);

						ProjectData.Target NewTarget = new ProjectData.Target(NewProject, Target.TargetRules.Name, Target.TargetRules.Type);

						foreach (UnrealTargetPlatform Platform in Platforms)
						{
							if (SupportedPlatforms.Contains(Platform))
							{
 								foreach (UnrealTargetConfiguration Config in Configs)
								{
									NewTarget.BuildProducts.Add(new ProjectData.BuildProduct
									{
										Platform = Platform,
										Config = Config,
										OutputType = ProjectData.EOutputType.Exe,
										OutputFile = GetExecutableFilename(Project, Target, Platform, Config, LinkEnvironment),
										CSharpInfo = null
									});
								}
							}
						}

						NewTarget.Defines.AddRange(CppEnvironment.Definitions);
						//NewTarget.Defines.AddRange(Project.IntelliSensePreprocessorDefinitions);
					}

					NewProject.IncludePaths = new List<string>();
					
					List<string> RawIncludes = new List<string>(Project.IntelliSenseIncludeSearchPaths);
					RawIncludes.AddRange(Project.IntelliSenseSystemIncludeSearchPaths);

					if (HostPlatform == UnrealTargetPlatform.Win64)
					{
						RawIncludes.AddRange(VCToolChain.GetVCIncludePaths(CppPlatform.Win64, WindowsPlatform.GetDefaultCompiler()).Trim(';').Split(';'));
					}

					foreach (string IncludePath in RawIncludes)
					{
						DirectoryReference AbsPath = DirectoryReference.Combine(Project.ProjectFilePath.Directory, IncludePath);
						if (DirectoryReference.Exists(AbsPath))
						{
							NewProject.IncludePaths.Add(AbsPath.ToString().Replace('\\', '/'));
						}
					}

					ProjectData.NativeProjects.Add(NewProject);
				}
				else
				{
					VCSharpProjectFile VCSharpProject = Project as VCSharpProjectFile;

					if (VCSharpProject.IsDotNETCoreProject() == BuildingForDotNetCore)
					{
						string ProjectName = Project.ProjectFilePath.GetFileNameWithoutExtension();

						ProjectData.Target Target = new ProjectData.Target(NewProject, ProjectName, TargetType.Program);

						UnrealTargetConfiguration[] Configs = { UnrealTargetConfiguration.Debug, UnrealTargetConfiguration.Development };

						foreach (UnrealTargetConfiguration Config in Configs)
						{
							CsProjectInfo Info = VCSharpProject.GetProjectInfo(Config);

							if (!Info.IsDotNETCoreProject() && Info.Properties.ContainsKey("OutputPath"))
							{
								ProjectData.EOutputType OutputType;
								string OutputTypeName;
								if (Info.Properties.TryGetValue("OutputType", out OutputTypeName))
								{
									OutputType = (ProjectData.EOutputType)Enum.Parse(typeof(ProjectData.EOutputType), OutputTypeName);
								}
								else
								{
									OutputType = ProjectData.EOutputType.Library;
								}

								if (OutputType == ProjectData.EOutputType.WinExe)
								{
									OutputType = ProjectData.EOutputType.Exe;
								}

								FileReference OutputFile = null;
								HashSet<FileReference> ProjectBuildProducts = new HashSet<FileReference>();
								Info.AddBuildProducts(DirectoryReference.Combine(VCSharpProject.ProjectFilePath.Directory, Info.Properties["OutputPath"]), ProjectBuildProducts);
								foreach (FileReference ProjectBuildProduct in ProjectBuildProducts)
								{
									if ((OutputType == ProjectData.EOutputType.Exe && ProjectBuildProduct.GetExtension() == FrameworkExecutableExtension) ||
										(OutputType == ProjectData.EOutputType.Library && ProjectBuildProduct.GetExtension() == FrameworkLibraryExtension))
									{
										OutputFile = ProjectBuildProduct;
										break;
									}
								}

								if (OutputFile != null)
								{
									Target.BuildProducts.Add(new ProjectData.BuildProduct
									{
										Platform = HostPlatform,
										Config = Config,
										OutputFile = OutputFile,
										OutputType = OutputType,
										CSharpInfo = Info
									});
								}
							}
						}

						ProjectData.CSharpProjects.Add(NewProject);
					}
				}
			}

			return ProjectData;
		}

		private void WriteProjectDataFile(ProjectData ProjectData)
		{
			JsonFile OutFile = new JsonFile();

			OutFile.BeginRootObject();
			OutFile.BeginArray("Projects");

			foreach (ProjectData.Project Project in ProjectData.AllProjects)
			{
				foreach (ProjectData.Target Target in Project.Targets)
				{
					OutFile.BeginObject();
					{
						OutFile.AddField("Name", Project.Name);
						OutFile.AddField("Type", Target.Type.ToString());

						if (Project.SourceProject is VCSharpProjectFile)
						{
							OutFile.AddField("ProjectFile", MakeWorkspaceRelativePath(Project.SourceProject.ProjectFilePath));
						}
						else
						{
							OutFile.BeginArray("IncludePaths");
							{
								foreach (string IncludePath in Project.IncludePaths)
								{
									OutFile.AddUnnamedField(IncludePath);
								}
							}
							OutFile.EndArray();
						}

						OutFile.BeginArray("Defines");
						{
							foreach (string Define in Target.Defines)
							{
								OutFile.AddUnnamedField(Define);
							}
						}
						OutFile.EndArray();

						OutFile.BeginArray("BuildProducts");
						{
							foreach (ProjectData.BuildProduct BuildProduct in Target.BuildProducts)
							{
								OutFile.BeginObject();
								{
									OutFile.AddField("Platform", BuildProduct.Platform.ToString());
									OutFile.AddField("Config", BuildProduct.Config.ToString());
									OutFile.AddField("OutputFile", MakeWorkspaceRelativePath(BuildProduct.OutputFile));
									OutFile.AddField("OutputType", BuildProduct.OutputType.ToString());
								}
								OutFile.EndObject();
							}
						}
						OutFile.EndArray();
					}
					OutFile.EndObject();
				}
			}

			OutFile.EndArray();
			OutFile.EndRootObject();
			OutFile.Write(FileReference.Combine(VSCodeDir, "unreal.json"));
		}

		private void WriteCppPropertiesFile(ProjectData Projects)
		{
			JsonFile OutFile = new JsonFile();

			HashSet<string> IncludePaths = new HashSet<string>();

			foreach (ProjectData.Project ProjectData in Projects.NativeProjects)
			{
				foreach (string IncludePath in ProjectData.SourceProject.IntelliSenseIncludeSearchPaths)
				{
					DirectoryReference AbsPath = DirectoryReference.Combine(ProjectData.SourceProject.ProjectFilePath.Directory, IncludePath);
					if (DirectoryReference.Exists(AbsPath))
					{
						IncludePaths.Add(AbsPath.ToString());
					}
				}
			}

			// NOTE: This needs to be expanded and improved so we can get system include paths in for other platforms
			if (HostPlatform == UnrealTargetPlatform.Win64)
			{
				string[] VCIncludePaths = VCToolChain.GetVCIncludePaths(CppPlatform.Win64, WindowsPlatform.GetDefaultCompiler()).Trim(';').Split(';');

				foreach (string VCIncludePath in VCIncludePaths)
				{
					if (!string.IsNullOrEmpty(VCIncludePath))
					{
						IncludePaths.Add(VCIncludePath);
					}
				}
			}

			OutFile.BeginRootObject();
			{
				OutFile.BeginArray("configurations");
				{
					OutFile.BeginObject();
					{
						OutFile.AddField("name", "UnrealEngine");

						OutFile.BeginArray("includePath");
						{
							foreach (var Path in IncludePaths)
							{
								OutFile.AddUnnamedField(Path.Replace('\\', '/'));
							}
						}
						OutFile.EndArray();

						OutFile.BeginArray("defines");
						{
							OutFile.AddUnnamedField("_DEBUG");
							OutFile.AddUnnamedField("UNICODE");
						}
						OutFile.EndArray();
					}
					OutFile.EndObject();
				}
				OutFile.EndArray();
			}
			OutFile.EndRootObject();

			OutFile.Write(FileReference.Combine(VSCodeDir, "c_cpp_properties.json"));
		}

		private class BuildProduct
		{
			public string ProjectName { get; set; }
			public UnrealTargetPlatform Platform { get; set; }
			public UnrealTargetConfiguration Config{ get; set; }
			public TargetType Type { get; set; }
			public FileReference OutputExecutable { get; set; }
		}

		private void WriteNativeTask(ProjectData.Project InProject, JsonFile OutFile)
		{
			string[] Commands = { "Build", "Clean" };

			foreach (ProjectData.Target Target in InProject.Targets)
			{
				foreach (ProjectData.BuildProduct BuildProduct in Target.BuildProducts)
				{
					foreach (string Command in Commands)
					{
						string TaskName = String.Format("{0} {1} {2} {3}", Target.Name, BuildProduct.Platform.ToString(), BuildProduct.Config, Command);
						List<string> ExtraParams = new List<string>();

						OutFile.BeginObject();
						{
							OutFile.AddField("taskName", TaskName);
							OutFile.AddField("group", "build");

							string CleanParam = Command == "Clean" ? "-clean" : null;

							if (BuildingForDotNetCore)
							{
								OutFile.AddField("command", "dotnet");
							}
							else
							{
								if (HostPlatform == UnrealTargetPlatform.Win64)
								{
									OutFile.AddField("command", "${workspaceRoot}/Engine/Build/BatchFiles/" + Command + ".bat");
									CleanParam = null;
								}
								else
								{
									OutFile.AddField("command", "${workspaceRoot}/Engine/Build/BatchFiles/" + HostPlatform.ToString() + "/Build.sh");

									if (Command == "Clean")
									{
										CleanParam = "-clean";
									}
								}
							}

							OutFile.BeginArray("args");
							{
								if (BuildingForDotNetCore)
								{
									OutFile.AddUnnamedField("${workspaceRoot}/Engine/Binaries/DotNET/UnrealBuildTool_NETCore.dll");
								}

								OutFile.AddUnnamedField(Target.Name);
								OutFile.AddUnnamedField(BuildProduct.Platform.ToString());
								OutFile.AddUnnamedField(BuildProduct.Config.ToString());
								OutFile.AddUnnamedField("-waitmutex");

								if (!string.IsNullOrEmpty(CleanParam))
								{
									OutFile.AddUnnamedField(CleanParam);
								}
							}
							OutFile.EndArray();
							OutFile.AddField("problemMatcher", "$msCompile");

							OutFile.BeginArray("dependsOn");
							{
								OutFile.AddUnnamedField("UnrealBuildTool " + HostPlatform.ToString() + " Development Build");
								if (Target.Type == TargetType.Editor)
								{
									OutFile.AddUnnamedField("ShaderCompileWorker " + HostPlatform.ToString() + " Development Build");
								}
							}
							OutFile.EndArray();

							if (!BuildingForDotNetCore)
							{
								OutFile.AddField("type", "shell");
							}
						}
						OutFile.EndObject();
					}
				}
			}
		}

		private void WriteCSharpTask(ProjectData.Project InProject, JsonFile OutFile)
		{
			VCSharpProjectFile ProjectFile = InProject.SourceProject as VCSharpProjectFile;
			bool bIsDotNetCore = ProjectFile.IsDotNETCoreProject();
			string[] Commands = { "Build", "Clean" };

			foreach (ProjectData.Target Target in InProject.Targets)
			{
				foreach (ProjectData.BuildProduct BuildProduct in Target.BuildProducts)
				{
					foreach (string Command in Commands)
					{
						string TaskName = String.Format("{0} {1} {2} {3}", Target.Name, BuildProduct.Platform, BuildProduct.Config, Command);

						OutFile.BeginObject();
						{
							OutFile.AddField("taskName", TaskName);
							OutFile.AddField("group", "build");
							if (bIsDotNetCore)
							{
								OutFile.AddField("command", "dotnet");
							}
							else
							{
								if (Utils.IsRunningOnMono)
								{
									OutFile.AddField("command", "xbuild");
								}
								else
								{
									OutFile.AddField("command", "${workspaceRoot}/Engine/Build/BatchFiles/MSBuild.bat");
								}
							}
							OutFile.BeginArray("args");
							{
								if (bIsDotNetCore)
								{
									OutFile.AddUnnamedField(Command.ToLower());
								}
								else
								{
									OutFile.AddUnnamedField("/t:" + Command.ToLower());
								}
								OutFile.AddUnnamedField(MakeWorkspaceRelativePath(InProject.SourceProject.ProjectFilePath));
								OutFile.AddUnnamedField("/p:GenerateFullPaths=true");
								if (HostPlatform == UnrealTargetPlatform.Win64)
								{
									OutFile.AddUnnamedField("/p:DebugType=portable");
								}
								OutFile.AddUnnamedField("/verbosity:minimal");

								if (bIsDotNetCore)
								{
									OutFile.AddUnnamedField("--configuration");
									OutFile.AddUnnamedField(BuildProduct.Config.ToString());
									OutFile.AddUnnamedField("--output");
									OutFile.AddUnnamedField(MakeWorkspaceRelativePath(BuildProduct.OutputFile.Directory));
								}
								else
								{
									OutFile.AddUnnamedField("/p:Configuration=" + BuildProduct.Config.ToString());
								}
							}
							OutFile.EndArray();
						}
						OutFile.AddField("problemMatcher", "$msCompile");

						if (!BuildingForDotNetCore)
						{
							OutFile.AddField("type", "shell");
						}
						OutFile.EndObject();
					}
				}
			}
		}

		private void WriteTasksFile(ProjectData ProjectData)
		{
			JsonFile OutFile = new JsonFile();

			OutFile.BeginRootObject();
			{
				OutFile.AddField("version", "2.0.0");

				OutFile.BeginArray("tasks");
				{
					foreach (ProjectData.Project NativeProject in ProjectData.NativeProjects)
					{
						WriteNativeTask(NativeProject, OutFile);
					}

					foreach (ProjectData.Project CSharpProject in ProjectData.CSharpProjects)
					{
						WriteCSharpTask(CSharpProject, OutFile);
					}

					OutFile.EndArray();
				}
			}
			OutFile.EndRootObject();

			OutFile.Write(FileReference.Combine(VSCodeDir, "tasks.json"));
		}
		
		public string EscapePath(string InputPath)
		{
			string Result = InputPath;
			if (Result.Contains(" "))
			{
				Result = "\"" + Result + "\"";
			}
			return Result;
		}

		private FileReference GetExecutableFilename(ProjectFile Project, ProjectTarget Target, UnrealTargetPlatform Platform, UnrealTargetConfiguration Configuration, TargetRules.LinkEnvironmentConfiguration LinkEnvironment)
		{
			TargetRules TargetRulesObject = Target.TargetRules;
			FileReference TargetFilePath = Target.TargetFilePath;
			string TargetName = TargetFilePath == null ? Project.ProjectFilePath.GetFileNameWithoutExtension() : TargetFilePath.GetFileNameWithoutAnyExtensions();
			string UBTPlatformName = Platform.ToString();
			string UBTConfigurationName = Configuration.ToString();

			string ProjectName = Project.ProjectFilePath.GetFileNameWithoutExtension();

			// Setup output path
			UEBuildPlatform BuildPlatform = UEBuildPlatform.GetBuildPlatform(Platform);

			// Figure out if this is a monolithic build
			bool bShouldCompileMonolithic = BuildPlatform.ShouldCompileMonolithicBinary(Platform);

			if (TargetRulesObject != null)
			{
				bShouldCompileMonolithic |= (Target.CreateRulesDelegate(Platform, Configuration).GetLegacyLinkType(Platform, Configuration) == TargetLinkType.Monolithic);
			}

			TargetType TargetRulesType = Target.TargetRules == null ? TargetType.Program : Target.TargetRules.Type;

			// Get the output directory
			DirectoryReference RootDirectory = UnrealBuildTool.EngineDirectory;
			if (TargetRulesType != TargetType.Program && (bShouldCompileMonolithic || TargetRulesObject.BuildEnvironment == TargetBuildEnvironment.Unique) && !TargetRulesObject.bOutputToEngineBinaries)
			{
				if(Target.UnrealProjectFilePath != null)
				{
					RootDirectory = Target.UnrealProjectFilePath.Directory;
				}
			}

			if (TargetRulesType == TargetType.Program && (TargetRulesObject == null || !TargetRulesObject.bOutputToEngineBinaries))
			{
				if(Target.UnrealProjectFilePath != null)
				{
					RootDirectory = Target.UnrealProjectFilePath.Directory;
				}
			}

			// Get the output directory
			DirectoryReference OutputDirectory = DirectoryReference.Combine(RootDirectory, "Binaries", UBTPlatformName);

			// Get the executable name (minus any platform or config suffixes)
			string BaseExeName = TargetName;
			if (!bShouldCompileMonolithic && TargetRulesType != TargetType.Program)
			{
				// Figure out what the compiled binary will be called so that we can point the IDE to the correct file
				string TargetConfigurationName = TargetRulesType.ToString();
				if (TargetConfigurationName != TargetType.Game.ToString() && TargetConfigurationName != TargetType.Program.ToString())
				{
					BaseExeName = "UE4" + TargetConfigurationName;
				}
			}

			// Make the output file path
			string ExecutableFilename = FileReference.Combine(OutputDirectory, BaseExeName).ToString();

			if ((Configuration != UnrealTargetConfiguration.Development) && ((Configuration !=  UnrealTargetConfiguration.DebugGame) || bShouldCompileMonolithic))
			{
				ExecutableFilename += "-" + UBTPlatformName + "-" + UBTConfigurationName;
			}
			ExecutableFilename += TargetRulesObject.Architecture;
			ExecutableFilename += BuildPlatform.GetBinaryExtension(UEBuildBinaryType.Executable);

			// Include the path to the actual executable for a Mac app bundle
			if (Platform == UnrealTargetPlatform.Mac && !LinkEnvironment.bIsBuildingConsoleApplication)
			{
				ExecutableFilename += ".app/Contents/MacOS/" + Path.GetFileName(ExecutableFilename);
			}

			return FileReference.MakeFromNormalizedFullPath(ExecutableFilename);
		}

		private void WriteNativeLaunchFile(ProjectData.Project InProject, JsonFile OutFile)
		{
			foreach (ProjectData.Target Target in InProject.Targets)
			{
				foreach (ProjectData.BuildProduct BuildProduct in Target.BuildProducts)
				{
					if (BuildProduct.Platform == HostPlatform)
					{
						string LaunchTaskName = String.Format("{0} {1} {2} Build", Target.Name, BuildProduct.Platform, BuildProduct.Config);

						OutFile.BeginObject();
						{
							OutFile.AddField("name", Target.Name + " (" + BuildProduct.Config.ToString() + ")");
							OutFile.AddField("request", "launch");
							OutFile.AddField("preLaunchTask", LaunchTaskName);
							OutFile.AddField("program", MakeWorkspaceRelativePath(BuildProduct.OutputFile));
							OutFile.BeginArray("args");
							{
								if (Target.Type == TargetRules.TargetType.Editor)
								{
									if (InProject.Name != "UE4")
									{
										OutFile.AddUnnamedField(InProject.Name);
									}

									if (BuildProduct.Config == UnrealTargetConfiguration.Debug || BuildProduct.Config == UnrealTargetConfiguration.DebugGame)
									{
										OutFile.AddUnnamedField("-debug");
									}
								}

							}
							OutFile.EndArray();
							OutFile.AddField("stopAtEntry", false);
							OutFile.AddField("cwd", MakeWorkspaceRelativePath(BuildProduct.OutputFile.Directory));
							OutFile.BeginArray("environment");
							{

							}
							OutFile.EndArray();
							OutFile.AddField("externalConsole", true);

							switch (HostPlatform)
							{
								case UnrealTargetPlatform.Win64:
									{
										OutFile.AddField("type", "cppvsdbg");
										OutFile.AddField("visualizerFile", "${workspaceRoot}/Engine/Extras/VisualStudioDebugging/UE4.natvis");
										break;
									}

								default:
									{
										OutFile.AddField("type", "cppdbg");
										OutFile.AddField("MIMode", "lldb");
										break;
									}
							}
						}
						OutFile.EndObject();
					}
				}
			}
		}

		private void WriteCSharpLaunchConfig(ProjectData.Project InProject, JsonFile OutFile)
		{
			VCSharpProjectFile CSharpProject = InProject.SourceProject as VCSharpProjectFile;
			bool bIsDotNetCore = CSharpProject.IsDotNETCoreProject();

			foreach (ProjectData.Target Target in InProject.Targets)
			{
				foreach (ProjectData.BuildProduct BuildProduct in Target.BuildProducts)
				{
					if (BuildProduct.OutputType == ProjectData.EOutputType.Exe)
					{
						string TaskName = String.Format("{0} ({1})", Target.Name, BuildProduct.Config);
						string BuildTaskName = String.Format("{0} {1} {2} Build", Target.Name, HostPlatform, BuildProduct.Config);

						OutFile.BeginObject();
						{
							OutFile.AddField("name", TaskName);

							if (bIsDotNetCore)
							{
								OutFile.AddField("type", "coreclr");
							}
							else
							{
								if (HostPlatform == UnrealTargetPlatform.Win64)
								{
									OutFile.AddField("type", "clr");
								}
								else
								{
									OutFile.AddField("type", "mono");
								}
							}

							OutFile.AddField("request", "launch");
							OutFile.AddField("preLaunchTask", BuildTaskName);

							if (bIsDotNetCore)
							{
								OutFile.AddField("program", "dotnet");
								OutFile.BeginArray("args");
								{
									OutFile.AddUnnamedField(MakeWorkspaceRelativePath(BuildProduct.OutputFile));
								}
								OutFile.EndArray();
								OutFile.AddField("externalConsole", true);
								OutFile.AddField("stopAtEntry", false);
							}
							else
							{
								OutFile.AddField("program", MakeWorkspaceRelativePath(BuildProduct.OutputFile));
								OutFile.BeginArray("args");
								{
								}
								OutFile.EndArray();

								if (HostPlatform == UnrealTargetPlatform.Win64)
								{
									OutFile.AddField("console", "externalTerminal");
								}
								else
								{
									OutFile.AddField("console", "internalConsole");
								}

								OutFile.AddField("internalConsoleOptions", "openOnSessionStart");
							}

							OutFile.AddField("cwd", "${workspaceRoot}");
						}
						OutFile.EndObject();
					}
				}
			}
		}

		private void WriteLaunchFile(ProjectData ProjectData)
		{
			JsonFile OutFile = new JsonFile();

			OutFile.BeginRootObject();
			{
				OutFile.AddField("version", "0.2.0");
				OutFile.BeginArray("configurations");
				{
					foreach (ProjectData.Project Project in ProjectData.NativeProjects)
					{
						WriteNativeLaunchFile(Project, OutFile);
					}

					foreach (ProjectData.Project Project in ProjectData.CSharpProjects)
					{
						WriteCSharpLaunchConfig(Project, OutFile);
					}
				}
				OutFile.EndArray();
			}
			OutFile.EndRootObject();

			OutFile.Write(FileReference.Combine(VSCodeDir, "launch.json"));
		}

		protected override void ConfigureProjectFileGeneration(string[] Arguments, ref bool IncludeAllPlatforms)
		{
			base.ConfigureProjectFileGeneration(Arguments, ref IncludeAllPlatforms);
		}

		private void WriteWorkspaceSettingsFile(List<ProjectFile> Projects)
		{
			JsonFile OutFile = new JsonFile();

			List<string> PathsToExclude = new List<string>();

			PathsToExclude.Add(".vs");
			PathsToExclude.Add("*.p4*");
			PathsToExclude.Add("**/*.png");
			PathsToExclude.Add("**/*.bat");
			PathsToExclude.Add("**/*.sh");
			PathsToExclude.Add("**/*.command");
			PathsToExclude.Add("**/*.sln");
			PathsToExclude.Add("**/*.db");
			PathsToExclude.Add("**/*.csproj.user");
			PathsToExclude.Add("**/*.relinked_action_ran");
			PathsToExclude.Add("**/*.vsscc");
			PathsToExclude.Add("**/*.link");
			PathsToExclude.Add("**/*.uprojectdirs");
			PathsToExclude.Add("packages");

			foreach (ProjectFile Project in Projects)
			{
				bool bFoundTarget = false;
				foreach (ProjectTarget Target in Project.ProjectTargets)
				{
					if (Target.TargetFilePath != null)
					{
						DirectoryReference ProjDir = Target.TargetFilePath.Directory.GetDirectoryName() == "Source" ? Target.TargetFilePath.Directory.ParentDirectory : Target.TargetFilePath.Directory;
						GetExcludePathsCPP(ProjDir, PathsToExclude);
						bFoundTarget = true;
					}
				}

				if (!bFoundTarget)
				{
					GetExcludePathsCSharp(Project.ProjectFilePath.Directory.ToString(), PathsToExclude);
				}
			}

			OutFile.BeginRootObject();
			{
				OutFile.AddField("typescript.tsc.autoDetect", "off");
				OutFile.BeginObject("files.exclude");
				{
					string WorkspaceRoot = UnrealBuildTool.RootDirectory.ToString().Replace('\\', '/') + "/";
					foreach (string PathToExclude in PathsToExclude)
					{
						OutFile.AddField(PathToExclude.Replace('\\', '/').Replace(WorkspaceRoot, ""), true);
					}
				}
				OutFile.EndObject();
			}
			OutFile.EndRootObject();

			OutFile.Write(FileReference.Combine(VSCodeDir, "settings.json"));
		}

		private void GetExcludePathsCPP(DirectoryReference BaseDir, List<string> PathsToExclude)
		{
			string[] DirWhiteList = { "Binaries", "Build", "Config", "Plugins", "Source", "Private", "Public", "Classes", "Resources" };
			foreach (DirectoryReference SubDir in DirectoryReference.EnumerateDirectories(BaseDir, "*", SearchOption.TopDirectoryOnly))
			{
				if (Array.Find(DirWhiteList, Dir => Dir == SubDir.GetDirectoryName()) == null)
				{
					string NewSubDir = SubDir.ToString();
					if (!PathsToExclude.Contains(NewSubDir))
					{
						PathsToExclude.Add(NewSubDir);
					}
				}
			}
		}

		private void GetExcludePathsCSharp(string BaseDir, List<string> PathsToExclude)
		{
			string[] BlackList =
			{
				"obj",
				"bin"
			};

			foreach (string BlackListDir in BlackList)
			{
				string ExcludePath = Path.Combine(BaseDir, BlackListDir);
				if (!PathsToExclude.Contains(ExcludePath))
				{
					PathsToExclude.Add(ExcludePath);
				}
			}
		}
	}
}