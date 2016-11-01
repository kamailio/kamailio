CREATE USER 'homer_user'@'localhost' IDENTIFIED BY 'homer_password';
GRANT ALL ON homer_configuration.* TO 'homer_user'@'localhost';
GRANT ALL ON homer_statistic.* TO 'homer_user'@'localhost';
GRANT ALL ON homer_data.* TO 'homer_user'@'localhost';
