// *****************************************
// *********** Form events

&AtServer
Procedure OnCreateAtServer ( Cancel, StandardProcessing )
	
	setUser ();
	filterByUser ();
	readAppearance ();
	Appearance.Apply ( ThisObject );
	
EndProcedure

&AtServer
Procedure readAppearance ()

	rules = new Array ();
	rules.Add ( "
	|User show empty ( UserFilter )
	|" );
	Appearance.Read ( ThisObject, rules );

EndProcedure

&AtServer
Procedure setUser ()
	
	UserFilter = SessionParameters.User;
	
EndProcedure

&AtServer
Procedure filterByUser ()
	
	DC.ChangeFilter ( List, "Session.User", UserFilter, not UserFilter.IsEmpty () );
	Appearance.Apply ( ThisObject, "UserFilter" );
	
EndProcedure 

// *****************************************
// *********** Group Form

&AtClient
Procedure UserFilterOnChange ( Item )
	
	filterByUser ();
	
EndProcedure
