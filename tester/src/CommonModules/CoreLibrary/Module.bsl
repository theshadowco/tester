
Function module ()
	
	return CoreExtension;
	
EndFunction

Procedure AdjustQuery ( Query ) export
	
	//@skip-warning
	Query.Text = module ().GetLibrary ( "Root" ).AdjustQuery ( Query.Text );
	
EndProcedure

Function QueryTables ( Query ) export
	
	//@skip-warning
	return Conversion.FromJSON ( module ().GetLibrary ( "Root" ).QueryTables ( Query ) );
	
EndFunction

Procedure AugmentQuery ( Query ) export
	
	//@skip-warning
	module ().GetLibrary ( "Root" ).AugmentQuery ( Query );
	
EndProcedure

Function ParseAppearance ( Rules ) export
	
	//@skip-warning
	result = Conversion.FromJSON ( module ().GetLibrary ( "Root" ).ParseAppearance ( StrConcat ( Rules, ";" ) ) );
	if ( TypeOf ( result ) = Type ( "Structure" ) ) then
		raise "Conditional Appearance " + result.Error + " at " + result.Position;
	endif;
	return new FixedArray ( result );
	
EndFunction

Function codemodule ()
	
	return CoreFunctions;
	
EndFunction

Function Condition1 ( Value1, Value2 ) export
	
	//@skip-warning
	return module ().GetLibrary ( "Collections" ).Condition1 ( Value1, Value2 );
	
EndFunction

Function Condition2 ( Value1, Value2, Value3 ) export
	
	//@skip-warning
	return module ().GetLibrary ( "Collections" ).Condition2 ( Value1, Value2, Value3 );
	
EndFunction

Function GetFileHash ( File ) export

	return module ().GetLibrary ( "Root" ).GetHash ( File );

EndFunction

Function GetStringHash ( String, AddBOM ) export

	return module ().GetLibrary ( "Root" ).GetStringHash ( String, AddBOM );

EndFunction

