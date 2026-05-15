function Settings () export

	address = Constants.MCPServer.Get ();
	parts = StrSplit ( address, ":" );
	if ( parts.Count () = 2 ) then
		return new Structure ( "Address, Port", parts [ 0 ], parts [ 1 ] );
	endif;

endfunction
