package steam;

abstract UID(hl.Bytes) {
	public function toString() {
		return this == null ? "NULL" : this.toBytes(8).toHex();
	}
}