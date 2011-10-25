--
-- DB scheme to store data recorded from APMR
-- Copyright (C) 2010 Stephen William Makonin. All rights reserved.
-- This project is here by released under the COMMON DEVELOPMENT AND DISTRIBUTION LICENSE (CDDL).
--
-- PostgreSQL database dump
--

SET statement_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = off;
SET check_function_bodies = false;
SET client_min_messages = warning;
SET escape_string_warning = off;

SET search_path = public, pg_catalog;

SET default_tablespace = '';

SET default_with_oids = false;

--
-- Name: rawdata; Type: TABLE; Schema: public; Owner: stephen; Tablespace: 
--

CREATE TABLE rawdata (
    home_id character varying(3) NOT NULL,
    device_id character varying(3) NOT NULL,
    gmt_ts timestamp without time zone NOT NULL,
    counter numeric(20,1),
    period_rate numeric(8,1),
    instantaneous_rate numeric(8,2) NOT NULL
);


ALTER TABLE public.rawdata OWNER TO stephen;

--
-- Name: rawdata_pkey; Type: CONSTRAINT; Schema: public; Owner: stephen; Tablespace: 
--

ALTER TABLE ONLY rawdata
    ADD CONSTRAINT rawdata_pkey PRIMARY KEY (home_id, device_id, gmt_ts);


--
-- Name: rawdata_pidx; Type: INDEX; Schema: public; Owner: stephen; Tablespace: 
--

CREATE UNIQUE INDEX rawdata_pidx ON rawdata USING btree (home_id, device_id, gmt_ts);

ALTER TABLE rawdata CLUSTER ON rawdata_pidx;


--
-- Name: rawdata; Type: ACL; Schema: public; Owner: stephen
--

REVOKE ALL ON TABLE rawdata FROM PUBLIC;
REVOKE ALL ON TABLE rawdata FROM stephen;
GRANT ALL ON TABLE rawdata TO stephen;
GRANT SELECT,INSERT ON TABLE rawdata TO restful;


--
-- PostgreSQL database dump complete
--

