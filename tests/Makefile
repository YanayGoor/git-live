all: $(NAME)

format:
	poetry run black .
	poetry run isort .
	poetry run autoflake .

lint:
	poetry run mypy .
	poetry run flake8 .

	rm -rf $(NAME) $(OBJ)

.PHONY: format lint