&AtServer
var GenerateOnOpen export;
&AtClient
var SelectedVariant;
&AtClient
var ChoiceForm;
&AtClient
var PreviousArea;
&AtClient
var WorkAroundSelectedActionParameters;
&AtClient
var WorkAroundDetails;
&AtClient
var TotalsEnv;
&AtServer
var StoredSettings;

// *****************************************
// *********** Form events

&AtServer
Procedure OnCreateAtServer ( Cancel, StandardProcessing )
	
	init ();
	uploadStoredSettings ();
	fix1cMetodologicalProblem ();
	applyParams ();
	getSchema ();

EndProcedure

&AtServer
Procedure OnLoadUserSettingsAtServer ( UserSettings, StandardSettingsUsed )

	events = reportManager ().Events ();
	if ( events.FullAccessRequest ) then
		SetPrivilegedMode ( reportManager ().FullAccessRequest ( ThisObject ) );
	endif;
	command = undefined;
	Parameters.Property ( "Command", command );
	if ( command = undefined ) then
		command = "OpenReport";
	endif;
	if ( command = "OpenReport" ) then
		openReport ();
	elsif ( command = "DrillDown" ) then
		drillDown ();
	elsif ( command = "Detail" ) then
		detail ();
	elsif ( command = "NewWindow" ) then
		openReportForNewWindow ();
	endif; 
	showStatus ();
	restoreSettingsButton ();
	afterLoadSettings ();
	if ( events.BeforeOpen
		and command = "OpenReport" ) then
		reportManager ().BeforeOpen ( Object.SettingsComposer, GenerateOnOpen );
	endif;
	if ( GenerateOnOpen ) then
		giveAChanceToChangeSettings ();
	endif; 
	setReportTitle ( ThisObject );
	setCurrentItem ();
	readAppearance ();
	Appearance.Apply ( ThisObject );

EndProcedure

&AtServer
Procedure giveAChanceToChangeSettings ()

	try
		makeReport ( false );
	except
		Message ( ErrorProcessing.BriefErrorDescription ( ErrorInfo () ) );
	endtry;

EndProcedure

&AtServer
Procedure readAppearance ()

	rules = new Array ();
	rules.Add ( "
	|GroupUserSettings show ShowSettings;
	|GroupQuickSettings show not ShowSettings;
	|CmdOpenSettings press ShowSettings;
	|ShowGrid press ShowGrid;
	|ShowHeaders press ShowHeaders;
	|ShowGrid ShowHeaders show not WebClient
	|" );
	Appearance.Read ( ThisObject, rules );

EndProcedure

&AtServer
Procedure init ()
	
	WebClient = SystemEnvironment.WebClient ();
	ReportName = reportObject ().Metadata ().Name;
	
EndProcedure

&AtServer
Procedure uploadStoredSettings ()

	Parameters.Property ( "Variant", StoredSettings );
	if ( StoredSettings = undefined ) then
		Parameters.Property ( "StoredSettings", StoredSettings );
	else
		Parameters.Variant = undefined;
	endif;

EndProcedure

&AtServer
Function reportObject ()
	
	return FormAttributeToValue ( "Object" );
	
EndFunction

&AtServer
Procedure fix1cMetodologicalProblem ()
	
	SetPrivilegedMode ( true );
	selection = SystemSettingsStorage.Select (
		new Structure ( "ObjectKey, User",
		"Report." + ReportName + "/Default/CurrentUserSettings", UserName () ) );
	while selection.Next() do
		SystemSettingsStorage.Delete(selection.ObjectKey, selection.SettingsKey, selection.User);
	enddo;

EndProcedure

&AtServer
Procedure applyParams ()
	
	SimpleReport = simpleReport ();
	OnDetailEvent = reportManager ().Events ().OnDetail;
	meta = reportObject ().Metadata ();
	ReportPresentation = ? ( meta.ExtendedPresentation = "", meta.Presentation (), meta.ExtendentPresentation );
	ReportRef = Catalogs.Metadata.Ref ( "Report." + ReportName );
	Parameters.Property ( "VariantPresentation", VariantPresentation );
	Parameters.Property ( "GenerateOnOpen", GenerateOnOpen );
	if ( GenerateOnOpen = undefined ) then
		GenerateOnOpen = false;
	endif; 
	setVariantAndSettings ();

EndProcedure

&AtServer
Procedure setVariantAndSettings ()

	if ( StoredSettings = undefined ) then
		if ( Parameters.Property ( "ReportVariant" ) ) then
			setReportVariant ( ThisObject, Parameters.ReportVariant );
		endif;
		if ( Parameters.Property ( "ReportSettings" ) ) then
			setReportSettings ( ThisObject, Parameters.ReportSettings );
		endif;
	else
		properties = StoredSettings.AdditionalProperties;
		if ( properties.Property ( "ReportVariant" ) ) then
			setReportVariant ( ThisObject, properties.ReportVariant );
		endif;
		if ( properties.Property ( "ReportSettings" ) ) then
			setReportSettings ( ThisObject, properties.ReportSettings );
		endif;
	endif;

EndProcedure

&AtServer
Function simpleReport ()
	
	report = ReportName;
	return report = "Scenarios";
		
EndFunction 

&AtServer
Function reportManager ()
	
	return Reports [ ReportName ];
	
EndFunction

&AtServer
Procedure getSchema ()
	
	dataSchema = Reporter.GetSchema ( ReportName );
	SchemaAddress = PutToTempStorage ( dataSchema, UUID );
		
EndProcedure 

&AtServer
Procedure openReport ()
	
	loadVariantServer ( ReportVariant, undefined );
	composer = Object.SettingsComposer;
	if ( StoredSettings = undefined ) then
		restorePeriod ();
		Reporter.ApplyFilters ( composer, Parameters );
	else
		composer.LoadSettings ( StoredSettings );
	endif;

EndProcedure

&AtServer
Procedure restorePeriod ()
	
	value = CommonSettingsStorage.Load ( periodSetting ( ThisObject ) );
	if ( value = undefined ) then
		return;
	endif;
	parameter = getPeriod ( ThisObject );
	if ( parameter = undefined
		or TypeOf ( parameter.Value ) <> TypeOf ( value ) ) then
		return;
	endif;
	parameter.Value = value;
	
EndProcedure

&AtClientAtServerNoContext
Function periodSetting ( Form )
	
	return "ReportPeriod/" + Form.ReportName;
	
EndFunction

&AtClientAtServerNoContext
Function getPeriod ( Form )
	
	variants = new Array ();
	variants.Add ( "Period" );
	variants.Add ( "AsOf" );
	variants.Add ( "ReportDate" );
	composer = Form.Object.SettingsComposer;
	for each item in variants do
		parameter = DC.FindParameter ( composer, item );
		if ( parameter <> undefined
			and parameter.UserSettingID <> "" ) then
			return parameter;
		endif;
	enddo;
	return undefined;
	
EndFunction

&AtServer
Procedure drillDown ()
	
	detailsProcess = new DataCompositionDetailsProcess ( GetFromTempStorage ( Parameters.DetailsDescription.Data ), new DataCompositionAvailableSettingsSource ( SchemaAddress ) );
	usedSettings = detailsProcess.ApplySettings ( Parameters.DetailsDescription.ID, Parameters.DetailsDescription.UsedSettings );
	if ( TypeOf ( usedSettings ) = Type ( "DataCompositionSettings" ) ) then
		Object.SettingsComposer.LoadSettings ( usedSettings );
	elsif ( TypeOf ( usedSettings ) = Type ( "DataCompositionUserSettings" ) ) then
		loadVariantServer ( ReportVariant, ReportSettings );
		Object.SettingsComposer.LoadUserSettings ( usedSettings );
	endif;
	
EndProcedure

&AtServer
Procedure detail ()
	
	loadVariantServer ( "#Default", undefined );
	Reports [ ReportName ].ApplyDetails ( Object.SettingsComposer, Parameters );
	
EndProcedure

&AtServer
Procedure openReportForNewWindow ()
	
	composer = Object.SettingsComposer;
	composer.LoadSettings ( StoredSettings );
	disableActualState ( Items.Result );
	
EndProcedure 

&AtServer
Procedure showStatus ()
	
	Items.Result.StatePresentation.Text = Output.ClickGenerateReport ();
	
EndProcedure

&AtServer
Procedure restoreSettingsButton ()
	
	ShowSettings = CommonSettingsStorage.Load ( "Report.Common", Enum.SettingsShowSettingsButtonState () );
	
EndProcedure 

&AtServer
Procedure setCurrentItem ()
	
	if ( GenerateOnOpen ) then
		setResultCurrentItem ( ThisObject );
		CurrentItem = Items.Result;
	else
		activateSettings ();
	endif; 
	
EndProcedure 

&AtClientAtServerNoContext
Procedure setResultCurrentItem ( Form )
	
	items = Form.Items;
	Form.CurrentItem = items.Result;
	
EndProcedure 

&AtServer
Procedure activateSettings ()
	
	if ( ShowSettings ) then
		CurrentItem = Items.UserSettings;
	else
		if ( Items.GroupQuickSettings.ChildItems.Count () > 0 ) then
			CurrentItem = Items.GroupQuickSettings.ChildItems [ 0 ];
		endif; 
	endif; 
	
EndProcedure 

&AtServer
Function makeReport ( Quickly )
	
	enableActualState ();
	Result.Clear ();
	report = prepareReport ();
	p = report.Params;
	p.Quickly = Quickly;
	if ( p.Events.OnCheck ) then
		cancel = false;
		report.OnCheck ( cancel );
		if ( cancel ) then
			return false;
		endif; 
	endif; 
	p.Settings = p.Composer.GetSettings ();
	if ( not Reporter.ComposeResult ( report ) ) then
		return false;
	endif; 
	storeDetailsData ( p );
	return true;
	
EndFunction

&AtServer
Function prepareReport ()
 	
	report = Reporter.Prepare ( ReportName );
	p = report.Params;
	p.GenerateOnOpen = ( GenerateOnOpen <> undefined ) and GenerateOnOpen;
	p.Variant = ReportVariant;
	p.Schema = reportObject ().DataCompositionSchema;
	p.Result = Result;
	p.Composer = Object.SettingsComposer;
	return report;
	
EndFunction 

&AtServer
Procedure storeDetailsData ( Params )
	
	if ( IsTempStorageURL ( DetailsAddress ) ) then
		DeleteFromTempStorage ( DetailsAddress );
		DetailsAddress = "";
	endif; 
	DetailsAddress = PutToTempStorage ( Params.Details, DetailsAddress );
	
EndProcedure 

&AtServer
Procedure enableActualState ()
	
	Items.Result.StatePresentation.AdditionalShowMode = AdditionalShowMode.DontUse;
	Items.Result.StatePresentation.Visible = false;
	
EndProcedure

&AtServer
Procedure afterLoadSettings ()
	
	if ( not ShowSettings ) then
		buildFilter ();
	endif; 
	
EndProcedure 

&AtClientAtServerNoContext
Procedure setReportTitle ( Form )
	
	object = Form.Object;
	report = Form.ReportName;
	composer = object.SettingsComposer;
	parts = new Array ();
	parts.Add ( Form.ReportPresentation );
	addPart ( parts, composer, "Period" );
	Form.Title = StrConcat ( parts, ", " );

EndProcedure 

&AtClientAtServerNoContext
Procedure addPart ( Parts, Composer, Fields )
	
	for each name in StrSplit ( Fields, ", " ) do
		value = DC.FindValue ( Composer, name );
		if ( value <> undefined ) then
			Parts.Add ( value );
		endif; 
	enddo;
	
EndProcedure 

&AtClient
Procedure BeforeClose ( Cancel, Exit, MessageText, StandardProcessing )
	
	if ( Exit ) then
		return;
	endif; 
	storePeriod ( ThisObject );
	if ( VariantModified ) then
		Cancel = true;
		Output.ReportVariantModified2 ( ThisObject, , , "SaveVariantBeforeClose" );
	endif;
	
EndProcedure

&AtClientAtServerNoContext
Procedure storePeriod ( Form )
	
	parameter = getPeriod ( Form );
	if ( parameter = undefined ) then
		return;
	endif;
	Logins.SaveSettings ( periodSetting ( Form ), , parameter.Value );
	
EndProcedure

&AtClient
Procedure SaveVariantBeforeClose ( Answer, Params ) export
	
	if ( Answer = DialogReturnCode.Yes ) then
		saveReportVariant ( "closeAfterSavingVariant" );
	elsif ( Answer = DialogReturnCode.No ) then
		VariantModified = false;
		Close ();
	endif; 

EndProcedure

&AtClient
Procedure closeAfterSavingVariant ( SavedSettings, IsSettings ) export
	
	if ( CommonSaveSettings ( SavedSettings, IsSettings ) ) then
		Close ();
	endif;
	
EndProcedure 

&AtClient
Procedure URLListGetProcessing ( URLList, DefaultKey )
	
	for each entry in URLList do
		entryKey = entry.Key;
		if ( entryKey = FormStandardURLVariant.Report ) then
			entry.Presentation = ReportPresentation;
		elsif ( entryKey = FormStandardURLVariant.ReportVariant ) then
			entry.Presentation = getVariantPresentation ();
		elsif ( entryKey = FormStandardURLVariant.ReportWithCurrentSettings ) then
			entry.Presentation = getReportPresentation ();
		endif;
	enddo;
	
EndProcedure

&AtClient
Function getVariantPresentation ()

	parts = new Array ();
	parts.Add ( ReportPresentation );
	parts.Add ( VariantPresentation );
	return StrConcat ( parts, ", " );

EndFunction

&AtClient
Function getReportPresentation ()
	
	parts = new Array ();
	parts.Add ( ReportPresentation );
	if ( TypeOf ( ReportVariant ) = Type ( "String" )
		and ReportVariant <> "#Default" ) then
		parts.Add ( VariantPresentation );
	endif;
	parameterType = Type ( "DataCompositionSettingsParameterValue" );
	filterType = Type ( "DataCompositionFilterItem" );
	filters = Object.SettingsComposer.Settings.Filter.Items;
	for each item in Object.SettingsComposer.UserSettings.Items do
		if ( TypeOf ( item ) = parameterType
			and item.Use
			and ValueIsFilled ( item.Value ) ) then
			parts.Add ( parameterPresentation ( item ) );
		elsif ( TypeOf ( item ) = filterType
			and item.Use
			and ValueIsFilled ( item.RightValue ) ) then
			for each filter in filters do
				if ( TypeOf ( filter ) = filterType 
					and filter.UserSettingID = item.UserSettingID ) then
					parts.Add ( filterPresentation ( filter, item ) );
					break;
				endif;
			enddo;
		endif;
	enddo;
	return StrConcat ( parts, ", " );
	
EndFunction

&AtClient
Function parameterPresentation ( Item )
	
	parameter = Object.SettingsComposer.Settings.DataParameters.AvailableParameters.Items.Find ( Item.Parameter );
	return parameter.Title + " = " + String ( Item.Value );
	
EndFunction

&AtClient
Function filterPresentation ( Filter, Item )
	
	type = Filter.ComparisonType;
	if ( type = DataCompositionComparisonType.Equal ) then
		comparison = "=";
	elsif ( type = DataCompositionComparisonType.NotEqual ) then
		comparison = "<>";
	else
		comparison = String ( type );
	endif;
	return String ( Filter.LeftValue ) + " " + comparison + " " + String ( Item.RightValue );
	
EndFunction

// *****************************************
// *********** Group Form

&AtClient
Procedure Make ( Command )
	
	buildReport ( false );
	
EndProcedure

&AtClient
Procedure buildReport ( Quickly )
	
	invalidateSelection ();
	if ( makeReport ( Quickly ) ) then
		setResultCurrentItem ( ThisObject );
	endif; 

EndProcedure 

&AtClient
Procedure invalidateSelection ()
	
	PreviousArea = undefined;
	
EndProcedure 

&AtClient
Procedure SendReportBySchedule ( Command )
	
	if ( not checkScheduling () ) then
		return;
	endif; 
	organizeSendingBySchedule ();
	
EndProcedure

&AtServer
Function checkScheduling ()
	
	class = Reports [ ReportName ];
	events = class.Events ();
	standardProcessing = true;
	cancel = false;
	if ( events.OnScheduling ) then
		class.OnScheduling ( Object.SettingsComposer, cancel, standardProcessing );
	endif; 
	if ( not cancel
		and standardProcessing ) then
		cancel = not checkPeriod ( Object.SettingsComposer, "Period" );
		if ( not cancel ) then
			cancel = not checkPeriod ( Object.SettingsComposer, "Asof" );
		endif; 
	endif; 
	return not cancel;
	
EndFunction 

&AtServer
Function checkPeriod ( Composer, Name )
	
	period = DC.FindParameter ( Composer, Name );
	if ( period <> undefined
		and period.Use
		and period.Value.Variant = StandardPeriodVariant.Custom ) then
		Output.ReportSchedulingIncorrectPeriod ();
		return false;
	endif; 
	return true;
	
EndFunction 

&AtClient
Procedure organizeSendingBySchedule ()
	
	values = new Structure ();
	values.Insert ( "Report", ReportRef );
	values.Insert ( "ReportVariant", ReportVariant );
	values.Insert ( "SettingsAddress", PutToTempStorage ( Object.SettingsComposer.UserSettings, UUID ) );
	p = new Structure ( "FillingValues", values );
	OpenForm ( "InformationRegister.ScheduledReports.RecordForm", p );
	
EndProcedure 

&AtClient
Procedure LoadVariant ( Command )
	
	loadReportVariant ();
	
EndProcedure

&AtClient
Procedure loadReportVariant ()
	
	showSettings ( false, false, "CommonLoadSettings" );
	
EndProcedure

&AtClient
Procedure showSettings ( IsSettings, IsSaving, Callback )
	
	p = new Structure ();
	address = PutToTempStorage ( ? ( IsSettings, Object.SettingsComposer.UserSettings, Object.SettingsComposer.Settings ), UUID );
	p.Insert ( "Report", ReportName );
	p.Insert ( "Settings", IsSettings );
	p.Insert ( "SettingsAddress", address );
	p.Insert ( "Saving", IsSaving );
	p.Insert ( "ReportVariant", ReportVariant );
	p.Insert ( "ReportSettings", ReportSettings );
	OpenForm ( "Catalog.ReportSettings.Form.SaveLoad", p, , , , , new CallbackDescription ( Callback, ThisObject, IsSettings ), FormWindowOpeningMode.LockWholeInterface );
	
EndProcedure

&AtClient
Procedure CommonLoadSettings ( SelectedItem, IsSettings ) export
	
	if ( SelectedItem = undefined ) then
		return;
	endif; 
	if ( IsSettings ) then
		applySettings ( SelectedItem );
	else
		if ( VariantModified ) then
			SelectedVariant = SelectedItem;
			Output.ReportVariantModified1 ( ThisObject, , , "LoadConfirmedVariant" );
		else
			applyVariant ( SelectedItem );
		endif; 
	endif;
	
EndProcedure 

&AtServer
Procedure applySettings ( val Setting )
	
	loadSettingsServer ( Setting );
	afterLoadSettings ();
	
EndProcedure 

&AtServer
Procedure loadSettingsServer ( Item )
	
	if ( IsTempStorageURL ( Item ) ) then
		settingsReport = GetFromTempStorage ( Item );
		if ( settingsReport = undefined ) then
			return;
		endif;
	else
		settingsReport = Item.Storage.Get ();
		setReportSettings ( ThisObject, Item );
	endif; 
	Object.SettingsComposer.LoadUserSettings ( settingsReport );
	disableActualState ( Items.Result );
	
EndProcedure

&AtClientAtServerNoContext
Procedure setReportSettings ( Form, Value )
	
	Form.ReportSettings = Value;
	Form.Object.SettingsComposer.Settings.AdditionalProperties.Insert ( "ReportSettings", Value );
	
EndProcedure

&AtClientAtServerNoContext
Procedure disableActualState ( Result )
	
	Result.StatePresentation.AdditionalShowMode = AdditionalShowMode.Irrelevance;
	Result.StatePresentation.Visible = true;
	
EndProcedure

&AtServer
Procedure applyVariant ( val Setting )
	
	loadVariantServer ( Setting, undefined );
	afterLoadSettings ();
	
EndProcedure 

&AtServer
Procedure loadVariantServer ( Variant, SettingsCode )
	
	if ( Variant = undefined ) then
		code = undefined;
	elsif ( TypeOf ( Variant ) = Type ( "CatalogRef.ReportSettings" ) ) then
		code = DF.Pick ( Variant, "Code" );
	else
		code = Variant;
	endif; 
	if ( code = undefined ) then
		loadDefaultSettings ();
		return;
	endif;
	if ( Left ( code, 1 ) = "#" ) then
		loadPredefinedVariant ( code );
	else
		loadUserVariant ( code );
	endif; 
	if ( ValueIsFilled ( SettingsCode ) ) then
		loadSettingsServer ( SettingsCode );
	else
		resetReportSettings ();
	endif;
	disableActualState ( Items.Result );

EndProcedure

&AtServer
Procedure loadDefaultSettings ()
	
	settingsReport = InformationRegisters.UsersReportSettings.Get ( new Structure ( "User, Report", SessionParameters.User, ReportRef ) );
	if ( ValueIsFilled ( settingsReport.Variant ) ) then
		variantCode = ? ( TypeOf ( settingsReport.Variant ) = Type ( "CatalogRef.ReportSettings" ), settingsReport.Variant.Code, settingsReport.Variant );
		loadVariantServer ( variantCode, settingsReport.Settings );
	else
		loadVariantServer ( "#Default", undefined );
	endif; 
	
EndProcedure

&AtServer
Procedure loadPredefinedVariant ( Code )
	
	dataSchema = Reporter.GetSchema ( ReportName );
	variants = dataSchema.SettingVariants;
	item = variants.Find ( Mid ( Code, 2 ) );
	if ( item = undefined ) then
		item = dataSchema.SettingVariants.Default;
		variant = "#Default";
	else
		variant = Code;
	endif; 
	VariantPresentation = item.Presentation;
	composer = Object.SettingsComposer;
	composer.LoadSettings ( item.Settings );
	composer.Refresh ();
	setReportVariant ( ThisObject, variant );
	
EndProcedure

&AtClientAtServerNoContext
Procedure setReportVariant ( Form, Value )
	
	Form.ReportVariant = Value;
	Form.Object.SettingsComposer.Settings.AdditionalProperties.Insert ( "ReportVariant", Value );
	
EndProcedure

&AtServer
Procedure loadUserVariant ( Code )
	
	ReportVariant = Catalogs.ReportSettings.FindByCode ( Code );
	VariantPresentation = "" + ReportVariant;
	composer = Object.SettingsComposer;
	composer.LoadSettings ( ReportVariant.Storage.Get () );
	composer.Refresh ();
	setReportVariant ( ThisObject, ReportVariant );
	
EndProcedure

&AtServer
Procedure resetReportSettings ()
	
	setReportSettings ( ThisObject, undefined );
	resetUserSettings ();
	
EndProcedure 

&AtClient
Procedure LoadConfirmedVariant ( Answer, Params ) export
	
	if ( Answer = DialogReturnCode.Yes ) then
		saveReportVariant ( "loadVariantAfterSavePrevious" );
	elsif ( Answer = DialogReturnCode.No ) then
		VariantModified = false;
		applyVariant ( SelectedVariant );
	endif; 

EndProcedure

&AtClient
Procedure loadVariantAfterSavePrevious ( SavedSettings, IsSettings ) export
	
	if ( CommonSaveSettings ( SavedSettings, IsSettings ) ) then
		applyVariant ( SelectedVariant );
	endif; 
	
EndProcedure 

&AtClient
Procedure LoadSettings ( Command )
	
	loadUserSettings ();
	
EndProcedure

&AtClient
Procedure loadUserSettings ()
	
	showSettings ( true, false, "CommonLoadSettings" );
	
EndProcedure

&AtClient
Procedure SaveVariant ( Command )
	
	saveReportVariant ( "CommonSaveSettings" );
	
EndProcedure

&AtClient
Procedure saveReportVariant ( ProcAfterSave )
	
	showSettings ( false, true, ProcAfterSave );
	
EndProcedure

&AtClient
Procedure SaveSettings ( Command )
	
	userSettingsSave ();
	
EndProcedure

&AtClient
Procedure userSettingsSave ()
	
	showSettings ( true, true, "CommonSaveSettings" );
	
EndProcedure

&AtClient
Function CommonSaveSettings ( SavedSettings, IsSettings ) export
	
	if ( TypeOf ( SavedSettings ) = Type ( "CatalogRef.ReportSettings" ) ) then
		if ( IsSettings ) then
			setReportSettings ( ThisObject, SavedSettings );
		else
			setReportVariant ( ThisObject, SavedSettings );
			VariantModified = false;
		endif; 
		return true;
	endif;
	return false;
	
EndFunction

&AtClient
Procedure OpenSettings ( Command )
	
	toggleSettings ();
	
EndProcedure

&AtServer
Procedure toggleSettings ()
	
	if ( ShowSettings ) then
		buildFilter ();
		ShowSettings = false;
	else
		ShowSettings = true;
	endif; 
	Appearance.Apply ( ThisObject, "ShowSettings" );
	activateSettings ();
	saveSettingsState ();
	
EndProcedure 

&AtServer
Procedure buildFilter ()
	
	clearFilter ();
	settings = Object.SettingsComposer.Settings;
	userSettings = Object.SettingsComposer.UserSettings.Items;
	list = settings.DataParameters.Items;
	availParams = settings.DataParameters.AvailableParameters;
	filters = settings.Filter.Items;
	availFilters = settings.Filter.FilterAvailableFields;
	parameterType = Type (  "DataCompositionSettingsParameterValue" );
	filterType = Type (  "DataCompositionFilterItem" );
	quick = DataCompositionSettingsItemViewMode.QuickAccess;
	for i = 0 to userSettings.Count () - 1 do
		item = userSettings [ i ];
		add = false;
		id = item.UserSettingID;
		itemType = TypeOf ( item );
		if ( itemType = parameterType ) then
			for each param in list do
				if ( param.UserSettingID = id
					and param.ViewMode = quick ) then
					label = availParams.FindParameter ( param.Parameter ).Title;
					add = true;
					break;
				endif;
			enddo; 
		elsif ( itemType = filterType ) then
			for each filter in filters do
				if ( filter.UserSettingID = id
					and filter.ViewMode = quick ) then
					label = filter.UserSettingPresentation;
					if ( label = "" ) then
						availItem = availFilters.FindField ( filter.LeftValue );
						if ( availItem = undefined ) then
							label = filter.LeftValue;
						else
							label = availItem.Title;
						endif; 
					endif; 
					add = true;
					break;
				endif; 
			enddo; 
		endif; 
		if ( add ) then
			adjustFilter ( item, itemType );
			drawFilter ( i, label );
		endif; 
	enddo; 
	
EndProcedure 

&AtServer
Procedure clearFilter ()
	
	fields = Items.GroupQuickSettings.ChildItems;
	i = fields.Count () - 1;
	while ( i >= 0 ) do
		Items.Delete ( fields.Get ( i ) );
		i = i - 1;
	enddo; 
	
EndProcedure 

&AtServer
Procedure adjustFilter ( Item, Type )
	
	if ( Item.Use ) then
		return;
	endif; 
	if ( Type = Type ( "DataCompositionFilterItem" ) ) then
		if ( ValueIsFilled ( Item.RightValue ) ) then
			Item.RightValue = undefined;
		endif; 
	elsif ( Type = Type ( "DataCompositionSettingsParameterValue" ) ) then
		if ( ValueIsFilled ( Item.Value ) ) then
			Item.Value = undefined;
		endif; 
	endif; 
	
EndProcedure 

&AtServer
Procedure drawFilter ( Index, Label )
	
	i = Format ( Index, "NZ=" );
	field = Items.Add ( "_" + i, Type ( "FormField" ), Items.GroupQuickSettings );
	field.DataPath = "Object.SettingsComposer.UserSettings[" + i + "].Value";
	field.Type = FormFieldType.InputField;
	field.Title = Label;
	field.Visible = true;
	field.OpenButton = false;
	field.OpenButton = false;
	field.ClearButton = true;
	if ( calendarFilter ( Index ) ) then
		field.ChoiceButtonRepresentation = ChoiceButtonRepresentation.ShowInDropListAndInInputField;
		field.CreateButton = false;
	endif;
	field.SetAction ( "OnChange", "FilterOnChange" );
	
EndProcedure 

&AtServer
Function calendarFilter ( Index )
	
	parameterType = Type ( "DataCompositionSettingsParameterValue" );
	composer = Object.SettingsComposer;
	setting = composer.UserSettings.Items [ Index ];
	if ( TypeOf ( setting ) <> parameterType ) then
		return false;
	endif;
	parameter = setting.Parameter;
	fixedList = composer.FixedSettings.DataParameters.Items;
	calendarType = Type ( "CatalogRef.Calendar" );
	for each item in fixedList do
		if ( TypeOf ( item ) = parameterType
			and item.Parameter = parameter ) then
			return TypeOf ( item.Value ) = calendarType;
		endif;
	enddo;
	return false;
	
EndFunction

&AtServer
Procedure saveSettingsState ()
	
	Logins.SaveSettings ( "Report.Common", Enum.SettingsShowSettingsButtonState (), ShowSettings );
	
EndProcedure 

&AtClient
Procedure ChangeVariant ( Command )
	
	changeReportVariant ();

EndProcedure

&AtClient
Procedure changeReportVariant ()
	
	p = new Structure ();
	p.Insert ( "Variant", Object.SettingsComposer.Settings );
	p.Insert ( "UserSettings", Object.SettingsComposer.UserSettings );
	p.Insert ( "VariantPresentation", VariantPresentation );
	ChoiceForm = GetForm ( "Report." + ReportName + ".VariantForm", p, , true );
	ChoiceForm.CallbackDescriptionOnClose = new CallbackDescription ( "LoadChangedVariant", ThisObject, ChoiceForm );
	ChoiceForm.WindowOpeningMode = FormWindowOpeningMode.LockWholeInterface;
	OpenForm ( ChoiceForm );
	
EndProcedure

&AtClient
Procedure LoadChangedVariant ( Result, Params ) export
	
	if ( Result = true ) then
		//@skip-warning
		Object.SettingsComposer = ChoiceForm.Report.SettingsComposer;
		#if ( not WebClient ) then
			disableActualState ( Items.Result );
		#endif
		VariantModified = true;
		afterLoadSettings ();
	endif; 
	
EndProcedure

&AtClient
Procedure ResetSettings ( Command )
	
	initSettings ();
	
EndProcedure

&AtServer
Procedure initSettings ()
	
	resetUserSettings ();
	afterLoadSettings ();
	
EndProcedure 

&AtServer
Procedure resetUserSettings ()
	
	Object.SettingsComposer.LoadUserSettings ( new DataCompositionUserSettings () );
	disableActualState ( Items.Result );
	
EndProcedure

&AtClient
Procedure NewWindow ( Command )
	
	openReportInNewWindow ();
	
EndProcedure

&AtClient
Procedure openReportInNewWindow ()
	
	p = ReportsSystem.GetParams ( ReportName );
	p.Command = "NewWindow";
	p.StoredSettings = getSettings ();
	p.Insert ( "VariantPresentation", VariantPresentation );
	ReportsSystem.Open ( p, , true );
	
EndProcedure

&AtServer
Function getSettings ()
	
	return Object.SettingsComposer.GetSettings ();
	
EndFunction

&AtClient
Procedure SetVariantAsDefault ( Command )
	
	setReportVariantAsDefault ();
	
EndProcedure

&AtClient
Procedure setReportVariantAsDefault ()
	
	setUserSettings ( false );
	
EndProcedure

&AtClient
Procedure SetSettingsAsDefault ( Command )
	
	setUserVariantAndSettingsAsDefault ();
	
EndProcedure

&AtServer
Procedure setUserVariantAndSettingsAsDefault ()
	
	setUserSettings ( false );
	setUserSettings ( true );
	
EndProcedure

&AtServer
Procedure setUserSettings ( IsSettings )
	
	record = InformationRegisters.UsersReportSettings.CreateRecordManager ();
	record.User = SessionParameters.User;
	record.Report = ReportRef;
	record.Read ();
	record.User = SessionParameters.User;
	record.Report = ReportRef;
	if ( IsSettings ) then
		record.Settings = ReportSettings;
	else
		record.Variant = ReportVariant;
	endif; 
	record.Write ();

EndProcedure

&AtClient
Procedure SelectReportVariant ( Command )
	
	loadReportVariant ();
	
EndProcedure

&AtClient
Procedure Help ( Command )
	
	OpenHelp ( "Report." + ReportName );
	
EndProcedure

// *****************************************
// *********** UserSettings

&AtClient
Procedure UserSettingsOnChange ( Item )
	
	applyUserSetting ( Object.SettingsComposer.UserSettings.GetObjectByID ( Items.UserSettings.CurrentRow ) );

EndProcedure

&AtClient
Procedure applyUserSetting ( Setting )
	
	updated = onChange ( Setting );
	#if ( not WebClient ) then
		if ( not updated ) then
			disableActualState ( Items.Result );
		endif; 
	#endif
	
EndProcedure 

&AtClient
Function onChange ( Setting )

	setReportTitle ( ThisObject );
	if ( SimpleReport ) then
		buildReport ( true );
		return true;
	endif; 
	return false;
	
EndFunction

&AtClient
Procedure FilterOnChange ( Item )
	
	setting = getSetting ( Item );
	fixComparison ( setting );
	applyUserSetting ( setting );
	
EndProcedure

&AtClient
Function getSetting ( Item )
	
	i = Number ( Mid ( Item.Name, 2 ) );
	setting = Object.SettingsComposer.UserSettings.Items [ i ];
	if ( TypeOf ( setting ) = Type ( "DataCompositionSettingsParameterValue" ) ) then
		setting.Use = ValueIsFilled ( setting.Value );
	else
		setting.Use = ValueIsFilled ( setting.RightValue );
	endif; 
	return setting;
	
EndFunction

&AtClient
Procedure fixComparison ( Setting )
	
	if ( TypeOf ( Setting ) <> Type ( "DataCompositionFilterItem" ) ) then
		return;
	endif;
	isFolder = isFolder ( Setting.RightValue );
	comparison = Setting.ComparisonType;
	if ( isFolder ) then
		if ( comparison = DataCompositionComparisonType.Equal ) then
			candidate = DataCompositionComparisonType.InHierarchy;
		elsif ( comparison = DataCompositionComparisonType.NotEqual ) then
			candidate = DataCompositionComparisonType.NotInHierarchy;
		else
			return;
		endif; 
	else
		if ( comparison = DataCompositionComparisonType.InHierarchy ) then
			candidate = DataCompositionComparisonType.Equal;
		elsif ( comparison = DataCompositionComparisonType.NotInHierarchy ) then
			candidate = DataCompositionComparisonType.NotEqual;
		else
			return;
		endif; 
	endif;
	Setting.ComparisonType = candidate;

EndProcedure

&AtServerNoContext
Function isFolder ( val Value )
	
	return Metafields.IsFolder ( Value );

EndFunction

&AtClient
Procedure UserSettingsValueOnChange ( Item )
	
	applyValue ( Object.SettingsComposer.UserSettings.GetObjectByID ( Items.UserSettings.CurrentRow ) );
	
EndProcedure

&AtClient
Procedure applyValue ( Setting )
	
	if ( TypeOf ( Setting ) = Type ( "DataCompositionFilterItem" ) ) then
		fixComparison ( Setting );
	endif;

EndProcedure

// *****************************************
// *********** Result

&AtClient
Procedure ResultDetailProcessing ( Item, Details, StandardProcessing )
	
	if ( TypeOf ( Details ) <> Type ( "DataCompositionDetailsID" ) ) then
		return;	
	endif;
	StandardProcessing = false;
	if ( OnDetailEvent ) then
		ReportDetails.Clear ();
		UseMainAction = false;
		DetailActions = undefined;
		beforeDetailing ( ReportName, ReportDetails, UseMainAction, DetailActions, DetailsAddress, SchemaAddress, Details );
	endif;
	doDetailProcessing ( Details, Item );
	
EndProcedure

&AtServerNoContext
Procedure beforeDetailing ( val ReportName, ReportDetails, UseMainAction, DetailActions, val DetailsAddress, val SchemaAddress, val Details )
	
	systemMenu = undefined;
	Reports [ ReportName ].OnDetail ( ReportDetails, systemMenu, UseMainAction, getFilters ( SchemaAddress, Details, DetailsAddress ) );
	if ( TypeOf ( systemMenu ) = Type ( "Array" ) ) then
		DetailActions = new FixedArray ( systemMenu );
	else
		DetailActions = systemMenu;
	endif;
	
EndProcedure

&AtClient
Procedure doDetailProcessing ( Details, Item )
	
	if ( DetailActions = undefined ) then
		actions = undefined;
	elsif ( DetailActions = null ) then
		actions = new Array ();
	else
		actions = new Array ( DetailActions );
	endif;
	detailsObject = new DataCompositionDetailsProcess ( DetailsAddress, new DataCompositionAvailableSettingsSource ( SchemaAddress ) );
	detailsObject.ShowActionChoice ( new CallbackDescription ( "ApplySelectedAction", ThisObject, Details ), Details, actions, ReportDetails,
	// "false or UseMainAction" - Bug workaround for webclient. It doesn't get UseMainAction as a boolean value if actions = undefined
	false or UseMainAction );

EndProcedure

&AtClient
Procedure ApplySelectedAction ( SelectedAction, SelectedActionParameters, Details ) export
	
	if ( SelectedAction = undefined
		or SelectedAction = DataCompositionDetailsProcessingAction.None ) then
		return;
	elsif ( SelectedAction = DataCompositionDetailsProcessingAction.OpenValue ) then
		ShowValue ( , SelectedActionParameters );
	elsif ( TypeOf ( SelectedAction ) = Type ( "String" ) ) then
 		p = ReportsSystem.GetParams ( SelectedAction );
		p.Command = "Detail";
		p.GenerateOnOpen = true;
		p.Parent = ReportName;
		p.Filters = getFilters ( SchemaAddress, Details, DetailsAddress );
		ReportsSystem.Open ( p, , true );
	else
		// Drilling down report requires idle handler. Otherwise, a new report's form
		// will be opened in a new window
		WorkAroundSelectedActionParameters = SelectedActionParameters;
		WorkAroundDetails = Details;
		AttachIdleHandler ( "detailReport", 0.01, true );
	endif; 
	
EndProcedure 

&AtServerNoContext
Function getFilters ( val Schema, val Details, val Address )
	
	settings = GetFromTempStorage ( Address );
	composer = new DataCompositionSettingsComposer ();
	composer.LoadSettings ( settings.Settings );
	composer.Initialize ( new DataCompositionAvailableSettingsSource ( Schema ) );
	return retrieveFilters ( composer, Details, settings );
	
EndFunction 

&AtServerNoContext
Function retrieveFilters ( Composer, Details, Settings )
	
	filters = new Array ();
	addDetails ( Settings.Items [ Details ], Composer, filters );
	clean ( filters );
	addFilters ( filters, Composer );
	return PutToTempStorage ( filters );
	
EndFunction

&AtServerNoContext
Procedure addDetails ( Item, Composer, Filters )
	
	if ( TypeOf ( Item ) = Type ( "DataCompositionFieldDetailsItem" ) ) then
		for each field in Item.GetFields () do
			allowedField = getAllowedField ( new DataCompositionField ( field.field ), Composer );
			if ( allowedField = undefined
				or allowedField.Resource ) then
				continue;
			endif;
			Filters.Add ( formalize ( field.Field, true, false, false, field ) );
		enddo;
	endif;
	for each parent in Item.GetParents() do
		addDetails ( parent, Composer, Filters );
	enddo;
	
EndProcedure

&AtServerNoContext
Function formalize ( Name, Field, Filter, Parameter, Item )
	
	data = new Structure ( "Name, Field, Filter, Parameter, Item, StandardProcessing, Comparison", Name, Field, Filter, Parameter, Item, true );
	if ( Field and Item.Hierarchy ) then
		data.Comparison = DataCompositionComparisonType.InHierarchy;
	endif; 
	return data;
	
EndFunction 

&AtServerNoContext
Function getAllowedField ( Field, Composer )
	
	if ( TypeOf ( Field ) = Type ( "String" ) ) then
		search = new DataCompositionField ( Field );
	else
		search = Field;
	endif;
	composerType = TypeOf ( Composer );
	if ( composerType = Type ( "DataCompositionSettingsComposer" )
	 or composerType = Type ( "DataCompositionDetailsData" )
	 or composerType = Type ( "DataCompositionNestedObjectSettings" ) ) then
		return Composer.Settings.SelectionAvailableFields.FindField ( search );
	else
		return Composer.FindField ( search );
	endif;
	
EndFunction

&AtServerNoContext
Procedure clean ( Filters )
	
	i = Filters.Count () - 1;
	while ( i >= 0 ) do
		name = Filters [ i ].Name;
		for j = 0 to i - 1 do
			if ( Filters [ j ].Name = name ) then
				Filters.Delete ( i );
				break;
			endif;
		enddo;
		k = 1;
		childKilled = false;
		while ( true ) do
			a = StrFind ( name, ".", , k );
			if ( a = 0 ) then
				break;
			endif; 
			parent = Left ( name, a - 1 );
			for j = 0 to Filters.Count () - 1 do
				if ( Filters [ j ].Name = parent ) then
					Filters.Delete ( i );
					childKilled = false;
					break;
				endif;
			enddo; 
			if ( childKilled ) then
				break;
			endif; 
			k = a + 1;
		enddo; 
		i = i - 1;
	enddo;
	
EndProcedure 

&AtServerNoContext
Procedure addFilters ( Filters, Composer )

	for each item in Composer.Settings.Filter.Items do
		if ( item.Use ) then
			Filters.Add ( formalize ( undefined, false, true, false, item ) );
		endif;
	enddo;
	for each item in Composer.Settings.DataParameters.Items do
		if ( item.Use ) then
			Filters.Add ( formalize ( String ( item.Parameter ), false, false, true, item ) );
		endif;
	enddo;

EndProcedure 

&AtClient
Procedure detailReport () export
	
	p = ReportsSystem.GetParams ( ReportName );
	p.Command = "DrillDown";
	p.ReportVariant = ReportVariant; 
	p.ReportSettings = ReportSettings; 
	p.GenerateOnOpen = true;
	p.Insert ( "VariantPresentation", VariantPresentation );
	p.Insert ( "DetailsDescription", new DataCompositionDetailsProcessDescription ( DetailsAddress, WorkAroundDetails, WorkAroundSelectedActionParameters ) );
	ReportsSystem.Open ( p, , true );
	
EndProcedure

&AtClient
Procedure ShowLevel ( Command )
	
	selectLevel ();
	
EndProcedure

&AtClient
Procedure selectLevel ()
	
	#if ( WebClient ) then
		top = 5;
	#else
		top = Result.RowGroupLevelCount ();
	#endif
	if ( top = 1 ) then
		return;
	endif; 
	menu = new ValueList ();
	for i = 1 to top do
		menu.Add ( i );
	enddo; 
	ShowChooseFromMenu ( new CallbackDescription ( "LevelSelected", ThisObject ), menu );
	
EndProcedure 

&AtClient
Procedure LevelSelected ( Value, Params ) export
	
	if ( Value = undefined ) then
		return;
	endif; 
	Result.ShowRowGroupLevel ( Value.Value - 1 );
	
EndProcedure 

&AtClient
Procedure ShowHeaders ( Command )
	
	toggleHeaders ();
	
EndProcedure

&AtClient
Procedure toggleHeaders ()
	
	ShowHeaders = not ShowHeaders;
	Items.Result.ShowHeaders = ShowHeaders;
	Appearance.Apply ( ThisObject, "ShowHeaders" );

EndProcedure

&AtClient
Procedure ShowGrid ( Command )
	
	toggleGrid ();
	
EndProcedure

&AtClient
Procedure toggleGrid ()
	
	ShowGrid = not ShowGrid;
	Items.Result.ShowGrid = ShowGrid;
	Appearance.Apply ( ThisObject, "ShowGrid" );

EndProcedure 

&AtClient
Procedure CalcTotals ( Command )
	
	updateTotals ( false );
	
EndProcedure

&AtClient
Procedure updateTotals ( CheckSquare )
	
	if ( TotalsEnv = undefined ) then
		SpreadsheetTotals.Init ( TotalsEnv );	
	endif;
	TotalsEnv.Spreadsheet = Result;
	TotalsEnv.CheckSquare = CheckSquare;
	SpreadsheetTotals.Update ( TotalsEnv );
	Items.CalcTotals.Visible = CheckSquare and TotalsEnv.HugeSquare;
	TotalInfo = TotalsEnv.Result; 
	
EndProcedure 

&AtClient
Procedure ResultOnActivateArea ( Item )

	if ( drawing ()
		or sameArea () ) then
		return;
	endif;
	startCalculation ();
	
EndProcedure

&AtClient
Function drawing ()
	
	return TypeOf ( Result.CurrentArea ) <> Type ( "SpreadsheetDocumentRange" );
	
EndFunction 

&AtClient
Function sameArea ()
	
	currentName = Result.CurrentArea.Name;
	if ( PreviousArea = currentName ) then
		return true;
	else
		PreviousArea = currentName;
		return false;
	endif; 
	
EndFunction 

&AtClient
Procedure startCalculation ()
	
	DetachIdleHandler ( "startUpdating" );
	AttachIdleHandler ( "startUpdating", 0.2, true );
	
EndProcedure 

&AtClient
Procedure startUpdating ()
	
	updateTotals ( true );
	
EndProcedure 
