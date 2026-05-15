
&AtClient
Procedure CommandProcessing ( Scenario, ExecuteParameters )
	
	runExplorer ( Scenario );
	
EndProcedure

&AtClient
Procedure runExplorer ( Scenario )
	
	var error;
	file = RepositoryFiles.ScenarioFile ( Scenario, error );
	if ( error <> undefined ) then
		Message ( error );
		return;
	endif;
	#if ( WebClient or MobileClient ) then
		Output.ClientDoesNotSupport ();
	#else
		RunApp ( FileSystem.GetParent ( file ) );
	#endif
	
EndProcedure
