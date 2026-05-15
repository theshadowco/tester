
// *****************************************
// *********** Form events

&AtServer
Procedure OnCreateAtServer ( Cancel, StandardProcessing )

	readAppearance ();

EndProcedure

&AtServer
Procedure readAppearance ()

	rules = new Array ();
	rules.Add ( "
	|#c MCPWorking show MCPWorking and not MCPRestartRequired;
	|#c MCPNotWorking hide MCPWorking or MCPRestartRequired;
	|#c MCPRestartRequired show MCPRestartRequired
	|" );
	Appearance.Read ( ThisObject, rules );

EndProcedure

&AtClient
Procedure AfterWrite ( WriteParameters )
	
	Notify ( Enum.MessageApplicationSettingsSaved () );
	
EndProcedure

&AtClient
Procedure OnOpen ( Cancel )
	
	checkMCP ();
	Appearance.Apply ( ThisObject );
	
EndProcedure

&AtClient
Procedure checkMCP ()

	MCPWorking = MCPD <> undefined;

EndProcedure

// *****************************************
// *********** Form

&AtClient
Procedure IDOnChange ( Item )

	adjustID ();

EndProcedure

// Server is used to make WebClient possible to use
&AtServer
Procedure adjustID ()
	
	id = Upper ( TrimAll ( ConstantsSet.ID ) );
	matches = Regexp.Select ( id, "[\d,[A-Z]+" );
	if ( matches.Count () = 0 ) then
		ConstantsSet.ID = "A000";
	else
		ConstantsSet.ID = matches [ 0 ].Value;
	endif;
	
EndProcedure

&AtClient
Procedure MCPServerOnChange ( Item )

	applyMCPServer ();
	
EndProcedure

&AtClient
Procedure applyMCPServer ()

	MCPRestartRequired = true;
	Appearance.Apply ( ThisForm, "MCPRestartRequired" );

EndProcedure