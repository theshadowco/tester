
Procedure AssembleTemplate ( Data, Scenario ) export
	
	error = "";
	tabDoc = getSpreadsheet ( Data, error );
	if ( tabDoc = undefined ) then
		Output.ScenarioTemplateLoadingError ( new Structure ( "Scenario, Error", Scenario, Error ) );
		return;
	endif;
	anchor = Max ( 1, tabDoc.TableHeight - 1 );
	areas = Scenario.Areas;
	signature = tabDoc.Area ( anchor, 1, anchor, 1 ).Text;
	if ( signature = RepositoryFiles.Signature () ) then
		table = areas.UnloadColumns ();
		anchor = anchor + 1;
		set = Conversion.FromJSON ( tabDoc.Area ( anchor, 1, anchor, 1 ).Text );
		for each row in set do
			newRow = table.Add ();
			FillPropertyValues ( newRow, row );
		enddo; 
		areas.Load ( table );
		tabDoc = tabDoc.GetArea ( "R1:R" + Format ( anchor - 2, "NG=" ) );
	else
		areas.Clear ();
	endif;
	Scenario.Template = new ValueStorage ( tabDoc );
	Scenario.Spreadsheet = true;
	
EndProcedure

Function getSpreadsheet ( Data, Error )
	
	storage = ? ( TypeOf ( Data ) = Type ( "BinaryData" ), Data, GetFromTempStorage ( Data ) );
	stream = storage.OpenStreamForRead ();
	tabDoc = new SpreadsheetDocument ();
	try
		tabDoc.Read ( stream );
	except
		Error = ErrorDescription ();
		return undefined;
	endtry;
	stream.Close ();
	return tabDoc;
	
EndFunction 

Procedure ResetTemplate ( Scenario ) export
	
	Scenario.Template = new ValueStorage ( new SpreadsheetDocument () );
	Scenario.Areas.Clear ();
	Scenario.Spreadsheet = false;

EndProcedure

Procedure Properties ( Data, Scenario ) export
	
	params = parseProperties ( Data );
	checkRequiredFields ( params );
	validateTree ( params.Tree );
	type = parseType ( params.Type );
	Scenario.Type = type;
	severity = parseSeverity ( params.Severity );
	Scenario.Severity = severity;
	Scenario.Tree = params.Tree;
	Scenario.Creator = getCreator ( params.Creator );
	Scenario.LastCreator = getCreator ( params.LastCreator );
	Scenario.Tag = Catalogs.TagKeys.Pick ( params.Tags );
	Scenario.Memo = params.Memo;
	
EndProcedure

Function parseProperties ( Data )
	
	try
		params = Conversion.FromJSON ( Data );
	except
		raise Output.ScenarioPropertiesNotJSON ();
	endtry;
	if ( TypeOf ( params ) <> Type ( "Structure" ) ) then
		raise Output.ScenarioPropertiesMissingRequiredFields (
			new Structure ( "Fields", requiredFields () ) );
	endif;
	return params;
	
EndFunction

Procedure checkRequiredFields ( Params )
	
	var value;
	missing = new Array ();
	list = Conversion.StringToArray ( requiredFields () );
	for each field in list do
		isMissing = not Params.Property ( field, value )
		or ( value = undefined and field <> "Severity" );
		if ( isMissing ) then
			missing.Add ( field );
		endif;
	enddo;
	if ( missing.Count () > 0 ) then
		raise Output.ScenarioPropertiesMissingRequiredFields (
			new Structure ( "Fields", StrConcat ( missing, ", " ) ) );
	endif;
	
EndProcedure

Function requiredFields ()
	
	return "Tree, Creator, LastCreator, Tags, Type, Severity, Memo";
	
EndFunction

Procedure validateTree ( Value )
	
	if ( TypeOf ( Value ) <> Type ( "Boolean" ) ) then
		raise Output.ScenarioPropertiesTreeMustBeBoolean ();
	endif;
	
EndProcedure

Function parseType ( Value )
	
	try
		result = Enums.Scenarios [ Value ];
	except
		result = undefined;
	endtry;
	if ( result = undefined ) then
		raise Output.ScenarioPropertiesWrongType (
			new Structure ( "Value", ? ( Value = undefined, "undefined", Value ) ) );
	endif;
	return result;
	
EndFunction

Function parseSeverity ( Value )
	
	if ( not ValueIsFilled ( Value ) ) then
		return Enums.Severity.EmptyRef ();
	endif;
	try
		result = Enums.Severity [ Value ];
	except
		raise Output.ScenarioPropertiesWrongSeverity (
			new Structure ( "Value", Value ) );
	endtry;
	return result;
	
EndFunction

Function getCreator ( User )
	
	ref = Catalogs.Users.FindByDescription ( User, true );
	if ( ref.IsEmpty () ) then
		obj = Catalogs.Users.CreateItem ();
		obj.Description = User;
		obj.AccessDenied = true;
		obj.DataExchange.Load = true;
		obj.Write ();
		ref = obj.Ref;
	endif;
	return ref;
	
EndFunction
