Function GetParams ( ReportName = undefined ) export
	
	p = new Structure ();
	setParams ( p, ReportName );
	return p;
	
EndFunction

Procedure setParams ( Params, ReportName )
	
	Params.Insert ( "ReportName", ReportName );
	Params.Insert ( "Command", "OpenReport" );
	Params.Insert ( "Filters" );
	Params.Insert ( "Parent" );
	Params.Insert ( "ReportVariant" );
	Params.Insert ( "ReportSettings" );
	Params.Insert ( "StoredSettings" );
	Params.Insert ( "GenerateOnOpen", false );
	
EndProcedure

&AtClient
Procedure Open ( Params, Owner = undefined, Unique = undefined,
	Window = undefined ) export

	OpenForm ( "Report." + Params.ReportName + ".Form", Params, Owner, Unique );

EndProcedure

&AtServer
Function URL ( Params ) export
	
	report = GetUrl ( Metadata.Reports [ Params.ReportName ],
		String ( new UUID () ), Params );
	return report;
		
EndFunction
