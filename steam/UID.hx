package steam;

abstract UID(hl.Bytes) {
	public function toString() {
		return this == null ? "NULL" : getBytes().toHex();
	}
	public function getBytes() {
		return this.toBytes(8);
	}
	@:op(a == b) static function __compare( a : UID, b : UID ) {
		return (cast a : hl.Bytes).compare(0, (cast b : hl.Bytes), 0, 8) == 0;
	}
}