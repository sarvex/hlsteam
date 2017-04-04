package steam;

abstract UID(hl.Bytes) {
	public function toString() {
		return this.toBytes(8).toHex();
	}
}