# RESTful web service to record data from APMR
# Copyright (C) 2010 Stephen William Makonin. All rights reserved.
# This project is here by released under the COMMON DEVELOPMENT AND DISTRIBUTION LICENSE (CDDL).

import sys, os, cgi, json, pg

post_text = ''.join(sys.stdin.readlines()) 
c = json.loads(post_text)['consumption']

con = pg.connect(dbname='consumption', host='localhost', user='restful', passwd='?????')

for m in c['meters']:

	sql = "SELECT COALESCE(counter, 0) AS last_val FROM rawdata WHERE device_id = '" + m['id'] + "' ORDER BY gmt_ts DESC LIMIT 1;" 
	for res in con.query(sql).dictresult():

		if m['id'] == 'RHE':
			continue

		m['period_rate'] = float(m['counter']) - float(res['last_val']);
		ts = str(c['at_year']).zfill(4) + '-' + str(c['at_month']).zfill(2) + '-' + str(c['at_day']).zfill(2) + ' ' + str(c['at_hour']).zfill(2) + ':' + str(c['at_minute']).zfill(2) + ':00'
 
		sql = "INSERT INTO rawdata (home_id, device_id, gmt_ts, counter, period_rate, instantaneous_rate) VALUES ('" + c['home'] + "', '" + m['id'] + "', '" + ts + "', " + str(m['counter']) + ", " + str(m['period_rate']) + ", " + str(m['instantaneous_rate']) + ");"
		#print sql
		con.query(sql)

con.close

