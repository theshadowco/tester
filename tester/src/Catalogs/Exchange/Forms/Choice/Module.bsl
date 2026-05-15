
&AtServer
Procedure OnCreateAtServer ( Cancel, StandardProcessing )
	
	Appearance.Read ( ThisObject );
	Appearance.Apply ( ThisObject );
	
EndProcedure