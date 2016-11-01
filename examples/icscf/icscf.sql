-- phpMyAdmin SQL Dump
-- version 4.4.13.1
-- http://www.phpmyadmin.net
--
-- Host: localhost
-- Generation Time: 17. Mrz 2016 um 17:36
-- Server version: 5.5.47-0+deb7u1
-- PHP Version: 5.4.45-0+deb7u2

SET SQL_MODE = "NO_AUTO_VALUE_ON_ZERO";
SET time_zone = "+00:00";

--
-- Database: `icscf`
--

-- --------------------------------------------------------

--
-- Tabellenstruktur für Tabelle `nds_trusted_domains`
--

CREATE TABLE IF NOT EXISTS `nds_trusted_domains` (
  `id` int(11) NOT NULL,
  `trusted_domain` varchar(83) NOT NULL DEFAULT ''
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- --------------------------------------------------------

--
-- Tabellenstruktur für Tabelle `s_cscf`
--

CREATE TABLE IF NOT EXISTS `s_cscf` (
  `id` int(11) NOT NULL,
  `name` varchar(83) NOT NULL DEFAULT '',
  `s_cscf_uri` varchar(83) NOT NULL DEFAULT ''
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- --------------------------------------------------------

--
-- Tabellenstruktur für Tabelle `s_cscf_capabilities`
--

CREATE TABLE IF NOT EXISTS `s_cscf_capabilities` (
  `id` int(11) NOT NULL,
  `id_s_cscf` int(11) NOT NULL DEFAULT '0',
  `capability` int(11) NOT NULL DEFAULT '0'
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

--
-- Indexes for dumped tables
--

--
-- Indexes for table `nds_trusted_domains`
--
ALTER TABLE `nds_trusted_domains`
  ADD PRIMARY KEY (`id`);

--
-- Indexes for table `s_cscf`
--
ALTER TABLE `s_cscf`
  ADD PRIMARY KEY (`id`);

--
-- Indexes for table `s_cscf_capabilities`
--
ALTER TABLE `s_cscf_capabilities`
  ADD PRIMARY KEY (`id`),
  ADD KEY `idx_capability` (`capability`),
  ADD KEY `idx_id_s_cscf` (`id_s_cscf`);

--
-- AUTO_INCREMENT for dumped tables
--

--
-- AUTO_INCREMENT for table `nds_trusted_domains`
--
ALTER TABLE `nds_trusted_domains`
  MODIFY `id` int(11) NOT NULL AUTO_INCREMENT;
--
-- AUTO_INCREMENT for table `s_cscf`
--
ALTER TABLE `s_cscf`
  MODIFY `id` int(11) NOT NULL AUTO_INCREMENT;
--
-- AUTO_INCREMENT for table `s_cscf_capabilities`
--
ALTER TABLE `s_cscf_capabilities`
  MODIFY `id` int(11) NOT NULL AUTO_INCREMENT;
