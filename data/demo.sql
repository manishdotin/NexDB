CREATE TABLE students (id INT, name TEXT, marks INT, active BOOL);
INSERT INTO students VALUES (101, "Rahul", 92, true);
INSERT INTO students VALUES (102, "Ananya", 87, true);
INSERT INTO students VALUES (103, "Vikram", 76, false);
SELECT * FROM students;
SELECT * FROM students WHERE marks >= 90;
UPDATE students SET marks = 95 WHERE id = 101;
DELETE FROM students WHERE id = 103;
.tables
.schema students
.save
