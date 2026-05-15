
Function FolderSuffix () export
	
	return ".dir";
	
EndFunction 

Function MXLFile () export
	
	return ".mxl";
	
EndFunction 

Function BSLFile () export
	
	return ".bsl";
	
EndFunction 

Function JSONFile () export
	
	return ".json";
	
EndFunction 

&AtClient
Function VSCodeWorkspace () export
	
	return ".code-workspace";
	
EndFunction 

&AtClient
Function Gitignore () export
	
	return ".gitignore";
	
EndFunction 

&AtClient
Function GitFolder () export
	
	return ".git";
	
EndFunction 

&AtClient
Function BSLServerSettings () export
	
	return ".bsl-language-server.json";
	
EndFunction 

&AtServer
Function Signature () export
	
	return "92b895ac-0620-4a28-a128-76e2bd1a5ca4";
	
EndFunction 

&AtClient
Function SystemFolder () export
	
	return ".tester";
	
EndFunction

Function ScenarioFile ( Scenario, Error = undefined ) export
	
	if ( TypeOf ( Scenario ) = Type ( "CatalogRef.Scenarios" ) ) then
		if ( Scenario.IsEmpty () ) then
			return undefined;
		endif;
		data = DF.Values ( Scenario, "Application, Path, Type, Tree" );
	else
		data = Scenario;
	endif;
	folder = scenarioFolder ( data );
	if ( folder = undefined ) then
		Error = Output.ScenarioApplicationUnmapped ( new Structure ( "Path", data.Path ) );
		return undefined;
	endif;
	slash = GetClientPathSeparator ();
	path = StrReplace ( data.Path, ".", slash );
	parts = StrSplit ( path, slash );
	name = parts [ parts.UBound() ];
	if ( data.Tree ) then
		return folder + slash + path + slash + name + RepositoryFiles.FolderSuffix ();
	else
		return folder + slash + path;
	endif;
	
EndFunction

Function scenarioFolder ( Object )

	application = Object.Application;
	#if ( Server ) then
		q = new Query ( "
		|select Folder as Folder
		|from ExchangePlan.Repositories
		|where not DeletionMark
		|and Session = &Session
		|and Application = &Application
		|" );
		q.SetParameter ( "Session", SessionParameters.Session );
		q.SetParameter ( "Application", application );
		table = q.Execute ().Unload ();
		return ? ( table.Count () = 0, undefined, table [ 0 ].Folder );
	#else
		if ( FoldersWatchdog = undefined ) then
			return undefined;
		endif;
		node = FoldersWatchdog [ application ];
		return ? ( node = undefined, undefined, node.Folder );
	#endif

EndFunction

&AtClient
Function FileToPath ( File, PathBegins ) export

	slash = GetPathSeparator ();
	name = FileSystem.GetFileName(File);
	id = Mid ( name, 1, StrFind ( name, "." ) - 1 );
	isFolder = StrFind ( name, RepositoryFiles.FolderSuffix () ) > 0;
	path = FileSystem.GetParent ( File ) + ? ( isFolder, "", slash + id );
	path = StrReplace ( Mid ( path, PathBegins ), slash, "." );
	return path;
	
EndFunction

Function FileToName ( File ) export
	
	name = FileSystem.GetFileName ( File );
	i = StrFind ( name, "." );
	return ? ( i = 0, name, Left ( name, i - 1 ) );
	
EndFunction

&AtClient
Procedure Sync ( Callback = undefined ) export
	
	MCPCall = Callback <> undefined;
	nothingToSync = FoldersWatchdog = undefined or FoldersWatchdog.Count () = 0;
	if ( nothingToSync )  then
		if ( MCPCall ) then
			RunCallback ( Callback );
		endif;
	else
		RepositoryFilesSynchingCallback = Callback;
		startSyncing ();
	endif;
	
EndProcedure

&AtClient
Procedure startSyncing ()
	
	GetForm ( "DataProcessor.Unload.Form", new Structure ( "Silent", true ) ).Proceed ();
	
EndProcedure
