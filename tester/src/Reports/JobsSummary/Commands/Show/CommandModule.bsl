&AtClient
Procedure CommandProcessing ( CommandParameter, CommandExecuteParameters )
	
	p = ReportsSystem.GetParams ( "JobsSummary" );
	p.GenerateOnOpen = true;
	ReportsSystem.Open ( p, CommandExecuteParameters.Source, true, CommandExecuteParameters.Window );
	
EndProcedure
