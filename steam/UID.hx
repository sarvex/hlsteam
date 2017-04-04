package steam;

abstract UID(hl.Bytes) {
	public function toString() {
		return this == null ? "NULL" : getBytes().toHex();
	}
	public function getBytes() {
		return this.toBytes(8);
	}
}