import Sys.println in log;

class GameServer {

	public static function main() {

		var config : Dynamic = haxe.Json.parse(sys.io.File.getContent("config.json"));

		if( !steam.GameServer.init(new sys.net.Host(config.host), config.port, config.gamePort, config.queryPort, Authentification, config.version) )
			throw "Can't init game server";

		steam.GameServer.logonAnonymous(function(ok) {
			if( !ok ) {
				log("Connection failed");
				Sys.exit(1);
			}
			log("LOGIN OK");
			steam.GameServer.enableHeartbeats(true);
		});

		GameClient.main();
	}

}


class GameClient {

	public static function main() {
		var app = Std.parseInt(sys.io.File.getContent("steam_appid.txt"));
		if( !steam.Api.init(app) )
			throw "Can't init steam client";
		log("Client connected");
	}

}
