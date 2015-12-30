
int __lookup_client_id(int zone_id) {
	int row;
	int column;
	row = (zone_id-1) / 5;

	column = (zone_id-1) % 5;

	return (row /2 ) + (5-column)  + 5 *((zone_id-1)/10);
}

int main(void) {
	int i;
	
	for (i=1; i<=30; i++ ){	
	__lookup_client_id(i);
		printf("ZONE: %d client :%d\n", i, __lookup_client_id(i));
	}
}
