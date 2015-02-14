{
	'targets': [
		{
			'target_name': 'mbus',
			'cflags': [
				'-Wall',
			],
			'ldflags': [
				'../libmbus/mbus/mbus-protocol-aux.o',
				'../libmbus/mbus/mbus-protocol.o',
				'../libmbus/mbus/mbus-serial.o,
				'../libmbus/mbus/mbus-tcp.o',
				'../libmbus/mbus/mbus.o'
			],
			'include_dirs': [
				'../libmodbus/mbus',
			],
			'sources': [
				'./src/main.cc'
			],
		},
	]
}
