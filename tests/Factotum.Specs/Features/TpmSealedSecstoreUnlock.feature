Feature: TPM sealed secstore unlock
  A secstore password can be sealed by the TPM and released only when the
  login keyring supplies the matching auth factor.

  Scenario: A sealed secstore password requires the login keyring secret
    Given a running factotum with the mock TPM backend
    And the login keyring secret is "login-secret"
    When I seal the secstore password "secstore-password"
    Then the sealed blob should not contain "secstore-password"
    And unsealing with "wrong-secret" should fail
    And unsealing with the login keyring secret should return "secstore-password"
