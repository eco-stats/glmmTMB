if(require("testthat")) {
    pkg <- "glmmTMB"
    require(pkg, character.only=TRUE)
    print(sessionInfo())
    test_package(pkg)
    print(warnings()) # TODO? catch most of these by expect_warning(..)
} else {
    warnings("Package 'testthat' not available, cannot run unit tests for package",
	     sQuote(pkg))
}