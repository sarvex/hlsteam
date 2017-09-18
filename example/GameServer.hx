import Sys.println in log;

class GameServer {

	public static function main() {

		if( Sys.args()[0] == "-client" ) {
			GameClient.main();
			return;
		}

		var config : Dynamic = haxe.Json.parse(sys.io.File.getContent("config.json"));
		var host = new sys.net.Host(config.host);

		if( !steam.GameServer.init(host, config.port, config.gamePort, config.queryPort, AuthentificationAndSecure, config.version) )
			throw "Can't init game server";

		log("Server started on " + host.toString() + ":" + config.port);

		steam.GameServer.setConfig(config.name, config.name, config.description);
		steam.GameServer.setInfo(4, false, config.name, 0, "DefaultMap");

		steam.GameServer.logonAnonymous(function(ok) {
			if( !ok ) {
				log("Connection failed");
				Sys.exit(1);
			}
			log("LOGIN OK");
			steam.GameServer.enableHeartbeats(true);
		});
	}

}


class GameClient {

	public static function main() {
		var app = Std.parseInt(sys.io.File.getContent("steam_appid.txt"));
		if( !steam.Api.init(app) )
			throw "Can't init steam client";
		log("Client connected");
		var config : Dynamic = haxe.Json.parse(sys.io.File.getContent("config.json"));
		steam.GameServer.requestInternetServerList(steam.Api.appId, {});
	}

}
